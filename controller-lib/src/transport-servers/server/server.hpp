#ifndef SERVER_HPP
#define SERVER_HPP
#include "session.hpp"
#include <netinet/ip.h>

/* Forward Declarations*/
namespace boost{
namespace asio{
    class io_context;
}
}

// Transport layer is dependent on boost/asio.
namespace server
{
    struct UnixAddress {
        int sock_type;
        int protocol;
        struct sockaddr_un address;
    };

    struct IPv4Address {
        int sock_type;
        int protocol;
        struct sockaddr_in address;
    };

    union Remote {
        struct {
            int sock_type;
            int protocol;
            struct sockaddr_storage address;
        } header;
        UnixAddress unix_addr;
        IPv4Address ipv4_addr;
    };


    // Servers own the underlying low level interface to the transport layer.
    // i.e. sockets, pipes, etc.
    // Servers accept incoming transport layer byte streams, and present
    // each unique stream as a Session.
    // Servers track the number of unique sessions, and maintain
    // the lifetime of sessions.
    class Server: public std::vector<std::shared_ptr<Session> >, public std::enable_shared_from_this<Server>
    {
    public:
        // Server constructors accept an asio io_context.
        Server(boost::asio::io_context& ioc): ioc_(ioc){}
        void run();
        void rm(const std::shared_ptr<Session>&);

        // Servers must implement a connect interface that returns a client 
        // session.
        virtual void async_connect(Remote addr, std::function<void(const boost::system::error_code&, const std::shared_ptr<Session>&)> fn) = 0;


        virtual ~Server() = default;
    protected:
        void acquire(){ mtx_.lock(); return; }
        void release(){ mtx_.unlock(); return; }
        boost::asio::io_context& ioc_;
    private:
        std::mutex mtx_;
        // Servers keep a reference to a global io_context.
    };
}
#endif