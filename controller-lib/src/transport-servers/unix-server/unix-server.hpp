#ifndef UNIX_SERVER_HPP
#define UNIX_SERVER_HPP
#include <boost/asio.hpp>
#include "../server/server.hpp"

namespace UnixServer{
    class unix_session : public server::Session
    {
    public:
        unix_session(boost::asio::io_context& ioc, server::Server& server): server::Session(server), socket_(ioc) {}
        unix_session(boost::asio::local::stream_protocol::socket&& socket, server::Server& server): server::Session(server), socket_(std::move(socket)) {}
        void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn) override;
        void async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn) override;
        void close() override;
        ~unix_session() {
            cancel();
        }
    
    private:
        boost::asio::local::stream_protocol::socket socket_;
    };

    class unix_server : public server::Server
    {
    public:
        unix_server(boost::asio::io_context& ioc);
        unix_server(boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint);

        std::shared_ptr<server::Session> async_connect(server::Remote addr, std::function<void(const boost::system::error_code&)> fn) override;

        void accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn);
        void stop() { acceptor_.close(); return; }
        ~unix_server();
            
    private:
        boost::asio::local::stream_protocol::endpoint endpoint_;
        boost::asio::local::stream_protocol::acceptor acceptor_;
    };

}//Namespace UnixServer
#endif