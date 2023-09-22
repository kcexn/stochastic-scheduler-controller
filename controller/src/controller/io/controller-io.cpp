#include "controller-io.hpp"
#include "../controller-events.hpp"
#include <transport-servers/sctp-server/sctp-session.hpp>
#include <ifaddrs.h>

#define SCTP_PORT 5300



namespace controller{
namespace io{
    IO::IO(std::shared_ptr<MessageBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        us_(ioc, boost::asio::local::stream_protocol::endpoint(local_endpoint)),
        ss_(ioc, transport::protocols::sctp::endpoint(transport::protocols::sctp::v4(), SCTP_PORT))
    { 
        /* Identify the local sctp server address. */
        // Start by hardcoding the local loop back network prefix.
        const char* network_prefix = "127.0.0.0/8";
        std::string subnet(network_prefix);
        std::size_t pos = subnet.find('/');
        if(pos == std::string::npos){
            throw "this shouldn't be possible";
        }
        std::string nprefix = subnet.substr(0, pos);
        //Convert the network prefix into an ipv4 address in network byte order.
        struct sockaddr_in npaddr;
        npaddr.sin_family = AF_INET;
        npaddr.sin_port = htons(SCTP_PORT);
        int ec = inet_aton(nprefix.c_str(), &npaddr.sin_addr);
        if(ec == -1){
            perror("inet_aton failed");
            throw "this shouldn't be possible.";
        }
        // Get the interface addresses.
        struct ifaddrs* ifah;
        if(getifaddrs(&ifah) == -1){
            perror("getifaddrs failed");
            throw "This shouldn't be possible.";
        }
        struct sockaddr_in laddr;
        laddr.sin_family = AF_INET;
        laddr.sin_port = htons(SCTP_PORT);
        for(struct ifaddrs* ifa = ifah; ifa != nullptr; ifa = ifa->ifa_next){
            if(ifa->ifa_addr->sa_family == AF_INET){
                struct sockaddr_in* ifaddr_in = (struct sockaddr_in*)(ifa->ifa_addr);
                if( (ifaddr_in->sin_addr.s_addr & npaddr.sin_addr.s_addr) == npaddr.sin_addr.s_addr){
                    /* Since the network prefix of the interface matches the prefix of the defined network, we select this interface to be the 
                        local address for SCTP. */
                    laddr.sin_addr = ifaddr_in->sin_addr;
                    break;
                }
            }
        }
        freeifaddrs(ifah);
        /* Set the local SCTP address to this structure. */
        local_sctp_address.ipv4_addr = {
            SOCK_SEQPACKET,
            IPPROTO_SCTP,
            laddr
        };

        std::thread io(
            &IO::start, this
        );
        io_ = io.native_handle();
        io.detach();
    }

    IO::IO(std::shared_ptr<MessageBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc, std::uint16_t sport)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        us_(ioc, boost::asio::local::stream_protocol::endpoint(local_endpoint)),
        ss_(ioc, transport::protocols::sctp::endpoint(transport::protocols::sctp::v4(), sport))
    { 
        /* Identify the local sctp server address. */
        const char* network_prefix;
        char* __OW_KUBENET = getenv("__OW_KUBENET");
        if(__OW_KUBENET == nullptr){
            network_prefix = "127.0.0.0/8";
        } else {
            network_prefix = __OW_KUBENET;
        }
        std::string subnet(network_prefix);
        std::size_t pos = subnet.find('/');
        if(pos == std::string::npos){
            throw "this shouldn't be possible";
        }
        std::string nprefix = subnet.substr(0, pos);
        //Convert the network prefix into an ipv4 address in network byte order.
        struct sockaddr_in npaddr;
        npaddr.sin_family = AF_INET;
        npaddr.sin_port = htons(sport);
        int ec = inet_aton(nprefix.c_str(), &npaddr.sin_addr);
        if(ec == -1){
            perror("inet_aton failed");
            throw "this shouldn't be possible.";
        }
        // Get the interface addresses.
        struct ifaddrs* ifah;
        if(getifaddrs(&ifah) == -1){
            perror("getifaddrs failed");
            throw "This shouldn't be possible.";
        }
        struct sockaddr_in laddr;
        laddr.sin_family = AF_INET;
        laddr.sin_port = htons(sport);
        for(struct ifaddrs* ifa = ifah; ifa != nullptr; ifa = ifa->ifa_next){
            if(ifa->ifa_addr->sa_family == AF_INET){
                struct sockaddr_in* ifaddr_in = (struct sockaddr_in*)(ifa->ifa_addr);
                if( (ifaddr_in->sin_addr.s_addr & npaddr.sin_addr.s_addr) == npaddr.sin_addr.s_addr){
                    /* Since the network prefix of the interface matches the prefix of the defined network, we select this interface to be the 
                        local address for SCTP. */
                    laddr.sin_addr = ifaddr_in->sin_addr;
                    break;
                }
            }
        }
        freeifaddrs(ifah);
        /* Set the local SCTP address to this structure. */
        local_sctp_address.ipv4_addr = {
            SOCK_SEQPACKET,
            IPPROTO_SCTP,
            laddr
        };

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