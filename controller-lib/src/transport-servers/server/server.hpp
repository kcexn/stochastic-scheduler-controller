#ifndef SERVER_HPP
#define SERVER_HPP
#include <boost/asio.hpp>
#include "session.hpp"

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
        void run(){ ioc_.run(); return; }
        
        template<typename Rep, typename Period>
        std::size_t run_for(const boost::asio::chrono::duration<Rep, Period>& rel_time) { return ioc_.run_for(rel_time); }
    private:
        // Servers keep a reference to a global io_context.
        boost::asio::io_context& ioc_;
    };
}
#endif