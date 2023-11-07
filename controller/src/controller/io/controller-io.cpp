#include "controller-io.hpp"
#include "../app/controller-app.hpp"
#include "../controller-events.hpp"
#include <transport-servers/sctp-server/sctp-session.hpp>
#include <ifaddrs.h>

#define SCTP_PORT 5300



namespace controller{
namespace io{
    IO::IO(std::shared_ptr<MessageBox> mbox, const std::string& local_endpoint, boost::asio::io_context& ioc, int* num_running_multi_handles, std::shared_ptr<libcurl::CurlMultiHandle> cmhp)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        ss_(ioc, transport::protocols::sctp::endpoint(transport::protocols::sctp::v4(), SCTP_PORT)),
        us_(ioc, boost::asio::local::stream_protocol::endpoint(local_endpoint)),
        num_running_multi_handles_{num_running_multi_handles},
        cmhp_{cmhp},
        stopped_{false}
    { 
        /* Identify the local sctp server address. */
        // Start by hardcoding the local loop back network prefix.
        const char* network_prefix = "127.0.0.0/8";
        std::string subnet(network_prefix);
        std::size_t pos = subnet.find('/');
        if(pos == std::string::npos){
            std::cerr << "controller-io.cpp:24:subnet doesn't have '/'." << std::endl;
            throw "this shouldn't be possible";
        }
        std::string nprefix = subnet.substr(0, pos);
        //Convert the network prefix into an ipv4 address in network byte order.
        struct sockaddr_in npaddr;
        npaddr.sin_family = AF_INET;
        npaddr.sin_port = htons(SCTP_PORT);
        int ec = inet_aton(nprefix.c_str(), &npaddr.sin_addr);
        if(ec == -1){
            std::cerr << "controller-io.cpp:34:inet_aton failed" << std::endl;
            throw "this shouldn't be possible.";
        }
        // Get the interface addresses.
        struct ifaddrs* ifah;
        if(getifaddrs(&ifah) == -1){
            std::cerr << "controller-io.cpp:34:getifaddrs failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
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
        try{
            std::thread io(
                &IO::start, this
            );
            io_ = io.native_handle();
            io.detach();
        } catch(std::system_error& e){
            std::cerr << "controller-io.cpp:70:io thread failed to start with error:" << e.what() << std::endl;
            throw e;
        }
    }

    IO::IO(std::shared_ptr<MessageBox> mbox, const std::string& local_endpoint, boost::asio::io_context& ioc, std::uint16_t sport, int* num_running_multi_handles, std::shared_ptr<libcurl::CurlMultiHandle> cmhp)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        ss_(ioc, transport::protocols::sctp::endpoint(transport::protocols::sctp::v4(), sport)),
        us_(ioc, boost::asio::local::stream_protocol::endpoint(local_endpoint)),
        num_running_multi_handles_{num_running_multi_handles},
        cmhp_{cmhp},
        stopped_{false}
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
            std::cerr << "subnet string doesn't have a / in it." << std::endl;
            throw "this shouldn't be possible";
        }
        std::string nprefix = subnet.substr(0, pos);
        //Convert the network prefix into an ipv4 address in network byte order.
        struct sockaddr_in npaddr;
        npaddr.sin_family = AF_INET;
        npaddr.sin_port = htons(sport);
        int ec = inet_aton(nprefix.c_str(), &npaddr.sin_addr);
        if(ec == -1){
            std::cerr << "controller-io.cpp:99:inet_aton failed." << std::endl;
            throw "this shouldn't be possible.";
        }
        // Get the interface addresses.
        struct ifaddrs* ifah;
        if(getifaddrs(&ifah) == -1){
            std::cerr << "controller-io.cpp:105:getifaddrs failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
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

        try{
            std::thread io(
                &IO::start, this
            );
            io_ = io.native_handle();
            io.detach();
        } catch(std::system_error& e){
            std::cerr << "controller-io.cpp:70:io thread failed to start with error:" << e.what() << std::endl;
            throw e;
        }
    }

    void IO::start(){
        std::shared_ptr<MessageBox> mbox = mbox_ptr_;
        us_.accept([&, mbox](const boost::system::error_code& ec, std::shared_ptr<UnixServer::unix_session> session){
            if (!ec){
                /* Callbacks are registered once with the session. The session will ensure that the callback is called everytime there is a read event
                   on the socket until the transport session is ultimately closed. */
                session->async_read([&, session, mbox](boost::system::error_code ec, std::size_t length){
                    if(!ec){
                        // struct timespec ts[2] = {};
                        // clock_gettime(CLOCK_REALTIME, &ts[0]);
                        // std::cout << "controller-io.cpp:149:" << (ts[0].tv_sec*1000 + ts[0].tv_nsec/1000000) << ":us_.session->async_read() started." << std::endl;
                        session->acquire_stream().write(session->buf().data(), length);
                        session->release_stream();
                        std::unique_lock<std::mutex> lk(*(mbox->sched_signal_mtx_ptr));
                        mbox->sched_signal_cv_ptr->wait(lk, [&](){ return !(mbox->msg_flag.load(std::memory_order::memory_order_relaxed)); });
                        mbox->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                        mbox->sched_signal_ptr->fetch_or(CTL_IO_READ_EVENT, std::memory_order::memory_order_relaxed);
                        mbox->session = session;
                        lk.unlock();
                        mbox->sched_signal_cv_ptr->notify_one();
                        // clock_gettime(CLOCK_REALTIME, &ts[1]);
                        // std::cout << "controller-io.cpp:162:" << (ts[1].tv_sec*1000 + ts[1].tv_nsec/1000000) << ":us_.session->async_read() finished." << std::endl;
                    } else {
                        if(ec != boost::asio::error::eof){
                            std::cerr << "Error in unix async read:" << ec.message() << std::endl;
                        }
                    }
                });
            } else {
                std::cerr << "Error in the acceptor callback: " << ec.message() << std::endl;
            }
        });

        ss_.init([&, mbox](const boost::system::error_code& ec,  std::shared_ptr<sctp_transport::SctpSession> session){
            if(!ec){
                // struct timespec ts[2] = {};
                // clock_gettime(CLOCK_REALTIME, &ts[0]);
                // std::cout << "controller-io.cpp:198:" << (ts[0].tv_sec*1000 + ts[0].tv_nsec/1000000) << ":ss_ callback started." << std::endl;
                std::unique_lock<std::mutex> lk(*(mbox->sched_signal_mtx_ptr));
                mbox->sched_signal_cv_ptr->wait(lk, [&](){ return !(mbox->msg_flag.load(std::memory_order::memory_order_relaxed)); });
                mbox->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                mbox->sched_signal_ptr->fetch_or(CTL_IO_READ_EVENT, std::memory_order::memory_order_relaxed);
                mbox->session = session;
                lk.unlock();
                mbox->sched_signal_cv_ptr->notify_one();          
                // clock_gettime(CLOCK_REALTIME, &ts[2]);
                // std::cout << "controller-io.cpp:209:" << (ts[2].tv_sec*1000 + ts[2].tv_nsec/1000000) << ":ss_ callback started." << std::endl;
                return;  
            } else {
                std::cerr << "Error in ss_.init()" << ec.message() << std::endl;
            }
        });
        std::chrono::milliseconds wake_period(10);
        while(!(mbox->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) & CTL_TERMINATE_EVENT)){
            ioc_.run_for(wake_period);
            // Strictly speaking there is a race condition here I think. But I don't think it's very important so I'll worry about it later.
            // TODO: make access and modification to the num_running_multi_handles_ value threadsafe.
            if(*num_running_multi_handles_ != 0){
                cmhp_->perform(num_running_multi_handles_);
            }
        }
        // std::cout << "controller-io.cpp:204:IO thread exiting" << std::endl;
        stopped_.store(true, std::memory_order::memory_order_relaxed);
        stop_cv_.notify_all();
        return;
    }

    void IO::stop(){
        ioc_.stop();
        us_.clear();
        ss_.clear();

        std::unique_lock<std::mutex> lk(stop_);
        stop_cv_.wait(lk, [&](){ return (stopped_.load(std::memory_order::memory_order_relaxed)); });
        lk.unlock();
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
                    break;
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