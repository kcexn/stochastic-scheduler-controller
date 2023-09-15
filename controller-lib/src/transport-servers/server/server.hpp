#ifndef SERVER_HPP
#define SERVER_HPP
#include "session.hpp"

/* Forward Declarations*/
namespace boost{
namespace asio{
    class io_context;
}
}

// Transport layer is dependent on boost/asio.
namespace server
{
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
        void acquire(){ mtx_.lock(); return; }
        void release(){ mtx_.unlock(); return; }

        virtual ~Server() = default;
    private:
        std::mutex mtx_;
        // Servers keep a reference to a global io_context.
        boost::asio::io_context& ioc_;
    };
}
#endif