#include "controller-io.hpp"
#include "../controller-events.hpp"
#include <transport-servers/sctp-server/sctp-session.hpp>

#ifdef DEBUG
#include <iostream>
#endif
namespace controller{
namespace io{
    IO::IO(std::shared_ptr<MessageBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        us_(ioc, boost::asio::local::stream_protocol::endpoint("/run/controller/controller.sock")),
        ss_(ioc, transport::protocols::sctp::endpoint(transport::protocols::sctp::v4(), 5200))
    { 
        std::thread io(
            &IO::start, this
        );
        io_ = io.native_handle();
        io.detach();
    }


    void IO::start(){
        std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> f = [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
            if (!ec){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), us_);
                /* Callbacks are registered once with the session. The session will ensure that the callback is called everytime there is a read event
                   on the socket until the transport session is ultimately closed. */
                session->async_read([&, session](boost::system::error_code ec, std::size_t length){
                    if(!ec){
                        session->acquire_stream().write(session->buf().data(), length);
                        session->release_stream();
                        std::unique_lock<std::mutex> lk(mbox_ptr_->mbx_mtx);
                        mbox_ptr_->mbx_cv.wait(lk, [&]{ return (mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)==false); });
                        mbox_ptr_->session = std::shared_ptr<UnixServer::unix_session>(session);
                        mbox_ptr_->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                        mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_READ_EVENT, std::memory_order::memory_order_relaxed);
                        lk.unlock();
                        mbox_ptr_->sched_signal_cv_ptr->notify_all();
                    }
                });
                us_.push_back(std::move(session));
                us_.accept(f);
            }
        };
        us_.accept(f);

        ss_.init([&](const boost::system::error_code& ec,  std::shared_ptr<sctp_transport::SctpSession> session){
            if(!ec){
                std::unique_lock<std::mutex> lk(mbox_ptr_->mbx_mtx);
                mbox_ptr_->mbx_cv.wait(lk, [&]{ return (mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)==false); });
                mbox_ptr_->session = std::static_pointer_cast<server::Session>(session);
                mbox_ptr_->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_READ_EVENT, std::memory_order::memory_order_relaxed);
                lk.unlock();
                mbox_ptr_->sched_signal_cv_ptr->notify_all();
            }
        });
        ioc_.run();
        pthread_exit(0);
    }

    void IO::stop(){
        ioc_.stop();
        us_.clear();
        ss_.clear();

        // Wait a reasonable amount of time for the IO thread to stop.
        struct timespec ts = {0,50000000};
        nanosleep(&ts, nullptr);

        ioc_.restart();
        while( ioc_.poll() > 0 ){}
    }

    void IO::async_connect(server::Remote rmt, std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)> fn)
    {
        /* Switch the async connection routing based on the transport domain labeling. */
        switch(rmt.header.address.ss_family)
        {
            case AF_UNIX:
            {
                /* This is for Unix Domain Sockets */
                us_.async_connect(rmt,fn);
                break;
            }
            case AF_INET:
            {
                /* This is IPv4 addresses */
                // Switch IPv4 routing based on socket type and protocol labeling.
                switch(rmt.ipv4_addr.sock_type)
                {
                    case SOCK_SEQPACKET:
                    {
                        switch(rmt.ipv4_addr.protocol)
                        {
                            case IPPROTO_SCTP:
                            {
                                ss_.async_connect(rmt, fn);
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    }

    IO::~IO()
    {
        stop();
    }
}//namespace io
}//namespace controller