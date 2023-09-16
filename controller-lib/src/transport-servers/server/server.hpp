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
    enum struct AddressType {
        TUPLE,
        HOSTNAME
    };

    struct NetworkTuple {
        AddressType type;
        struct sockaddr_in local_addr;
        int protocol;
        struct sockaddr_in remote_addr;
    };

    struct Hostname {
        AddressType type;
        struct sockaddr_in local_addr;
        int protocol;
        const char* name;
    };

    union Remote {
        struct {
            // The method by which we are identifying the remote connection point.
            AddressType type;
            // the local endpoint that we are connecting from.
            // for now, we are ignoring this.
            struct sockaddr_in local_addr;
            int protocol;
        } header;
        NetworkTuple tuple;
        Hostname hostname;
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
        virtual std::shared_ptr<Session> async_connect(Remote addr, std::function<void(const boost::system::error_code&)> fn) = 0;


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