#include "controller-io.hpp"
#include <transport-servers/sctp-server/sctp-session.hpp>
#include "../../echo-app/utils/common.hpp"

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
        #ifdef DEBUG
        std::cout << "Endpoint Parameterized IO Constructor!" << std::endl;
        #endif
        std::thread io(
            &IO::start, this
        );
        io_ = io.native_handle();
        io.detach();
    }


    void IO::start(){
        #ifdef DEBUG
        std::cout << "IO Thread started!" << std::endl;
        #endif
        std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> f = [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
            if (!ec){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), us_);
                std::function<void(boost::system::error_code ec, std::size_t length)> g = [&, session](boost::system::error_code ec, std::size_t length){
                    session->acquire_stream().write(session->buf().data(), length);
                    session->release_stream();
                    std::unique_lock<std::mutex> lk(mbox_ptr_->mbx_mtx);
                    mbox_ptr_->mbx_cv.wait(lk, [&]{ return (mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)==false || mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed) !=0); });
                    int thread_local_signal = mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed);
                    mbox_ptr_->session = std::shared_ptr<UnixServer::unix_session>(session);
                    mbox_ptr_->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                    mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::UNIX_READ, std::memory_order::memory_order_relaxed);
                    lk.unlock();
                    mbox_ptr_->sched_signal_cv_ptr->notify_all();
                    if((thread_local_signal&echo::Signals::TERMINATE) == echo::Signals::TERMINATE){
                        pthread_exit(0);
                    }
                };
                session->async_read(g);
                us_.push_back(std::move(session));
                us_.accept(f);
            }
        };
        us_.accept(f);

        std::function<void(const boost::system::error_code& ec,  std::shared_ptr<sctp_transport::SctpSession> session)> g = [&](const boost::system::error_code& ec,  std::shared_ptr<sctp_transport::SctpSession> session){
            if(!ec){
                std::unique_lock<std::mutex> lk(mbox_ptr_->mbx_mtx);
                mbox_ptr_->mbx_cv.wait(lk, [&]{ return (mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)==false || mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed) !=0); });
                int thread_local_signal = mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed);
                mbox_ptr_->session = std::static_pointer_cast<server::Session>(session);
                mbox_ptr_->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::UNIX_READ, std::memory_order::memory_order_relaxed);
                lk.unlock();
                mbox_ptr_->sched_signal_cv_ptr->notify_all();
                if((thread_local_signal&echo::Signals::TERMINATE) == echo::Signals::TERMINATE){
                    pthread_exit(0);
                }
            }
        };
        ss_.init(g);

        ioc_.run();
        #ifdef DEBUG
        std::cout << "IO thread has stopped." << std::endl;
        #endif
        pthread_exit(0);
    }

    void IO::stop(){
        ioc_.stop();
        us_.clear();

        // Wait a reasonable amount of time for the IO thread to stop.
        struct timespec ts = {0,50000000};
        nanosleep(&ts, nullptr);

        ioc_.restart();
        while( ioc_.poll() > 0 ){}
    }

    IO::~IO()
    {
        #ifdef DEBUG
        std::cout << "IO Destructor!" << std::endl;
        #endif
        stop();
    }
}//namespace io
}//namespace controller