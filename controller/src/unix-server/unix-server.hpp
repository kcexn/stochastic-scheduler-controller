#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include <array>
#include <functional>
#include <sstream>
#include <boost/asio.hpp>

#ifdef DEBUG
#include <iostream>
#endif

namespace UnixServer{
    class Session
      : public std::enable_shared_from_this<Session>
    {
    public:
        enum { max_length = 65535 };

        Session(boost::asio::local::stream_protocol::socket socket);
        #ifdef DEBUG
        ~Session();
        #endif


        void start();
        void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn);
        void do_write(std::size_t length);
        void cancel() { cancel_signal_.emit(boost::asio::cancellation_type::total); }
        void shutdown_read();
        void shutdown_write();
        void close();
        std::size_t& buflen() { return buflen_;}
        std::array<char, max_length>& sockbuf() { return sockbuf_; }
        std::stringstream& stream() {return stream_; }

        boost::asio::local::stream_protocol::socket& socket(){ return socket_; }

        bool operator==(const Session& other) const {
            return this == &other;
        }
    private:
        boost::asio::local::stream_protocol::socket socket_;
        std::size_t buflen_ = 0;
        std::array<char, max_length> sockbuf_;
        std::stringstream stream_;
        char data_[max_length];
        boost::asio::cancellation_signal cancel_signal_;
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