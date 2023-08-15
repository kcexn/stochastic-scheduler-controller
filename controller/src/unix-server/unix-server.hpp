#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include <functional>
#include <boost/asio.hpp>

#ifdef DEBUG
#include <iostream>
#endif

namespace UnixServer{
    class Session
      : public std::enable_shared_from_this<Session>
    {
    public:
        Session(boost::asio::local::stream_protocol::socket socket);
        void start();
        void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn);
        void do_write(std::size_t length);
        void shutdown_read();
        void shutdown_write();
        void close();
        std::size_t& buflen() { return buflen_;}
    private:
        boost::asio::local::stream_protocol::socket socket_;
        enum { max_length = 1024 };
        std::size_t buflen_;
        char data_[max_length];
    };

    class Server{
        public:
            Server(boost::asio::io_context& ioc);
            void start_accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn);
            ~Server();
        private:  
            boost::asio::io_context& ioc_;
            boost::asio::local::stream_protocol::acceptor acceptor_;
    };

}//Namespace UnixServer
#endif