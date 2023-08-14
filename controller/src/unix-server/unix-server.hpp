#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
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
    private:
        void do_read();
        void do_write(std::size_t length);
        boost::asio::local::stream_protocol::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
    };

    class Server{
        public:
            Server(boost::asio::io_context& ioc);
        private:
            void start_accept();
            boost::asio::io_context& ioc_;
            boost::asio::local::stream_protocol::acceptor acceptor_;
    };

}//Namespace UnixServer
#endif