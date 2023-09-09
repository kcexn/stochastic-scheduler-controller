#include "unix-server.hpp"
#include <cstring>
#include <filesystem>
#include <ios>

namespace UnixServer{
    //-------------------------------------|
    //Unix-Server Sessions.
    Session::Session(boost::asio::local::stream_protocol::socket socket)
        : socket_(std::move(socket)),
          stream_(std::ios_base::in | std::ios_base::out | std::ios_base::app )
    {
        #ifdef DEBUG
        std::cout << "Unix Session Constructor!" << std::endl;
        #endif
    }

    Session::Session( Session&& other )
      : socket_(std::move(other.socket_)),
        stream_(std::move(other.stream_))
    {
        #ifdef DEBUG
        std::cout << "Unix Session Move Constructor!." << std::endl;
        #endif
    }

    Session::~Session()
    {
        #ifdef DEBUG
        std::cout << "Unix Session Destructor!" << std::endl;
        #endif
        close();
    }
        
    void Session::async_read(
        std::function<void(boost::system::error_code ec, std::size_t length)> fn
        )
    {
        socket_.async_read_some(
            boost::asio::buffer(sockbuf_.data(), max_length),
            boost::asio::bind_cancellation_slot(
                stop_signal_.slot(),
                fn
            )
        );
    }

    void Session::do_write(std::size_t length){
        socket_.write_some(boost::asio::buffer(sockbuf_.data(), length));
    }

    void Session::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void(Session&)>& fn){
        std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
        std::memcpy(write_data_ptr->data(), write_buffer.data(), write_data_ptr->size());
        boost::asio::const_buffer buf(write_data_ptr->data(), write_data_ptr->size());
        socket_.async_write_some(
            buf,
            [&, buf, write_data_ptr, fn](const boost::system::error_code& ec, std::size_t bytes_transferred){
                if (!ec){
                    std::size_t remaining_bytes = write_data_ptr->size() - bytes_transferred;
                    if ( remaining_bytes > 0 ){
                        boost::asio::const_buffer buf = buf + bytes_transferred;
                        async_write(buf, fn);
                    } else {
                        // Once the write is complete execute the completion
                        // handler.
                        fn(*this);
                    }
                }
            }
        );
    }

    void Session::shutdown_read(){
        #ifdef DEBUG
        std::cout << "Shutdown the Read side of the Unix socket." << std::endl;
        #endif
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_receive, ec);
    }

    void Session::shutdown_write(){
        #ifdef DEBUG
        std::cout << "Shutdown the write side of the Unix socket." << std::endl;
        #endif
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_send, ec);
    }

    void Session::close(){
        #ifdef DEBUG
        std::cout << "Shutdown both sides of the Unix socket. Close the Unix socket." << std::endl;
        #endif
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_both, ec);
        socket_.close(ec);
    }
    //-----------------------------------------|
    //Unix-Server Server
    Server::Server(boost::asio::io_context& ioc)
      : ioc_(ioc),
        acceptor_(ioc, boost::asio::local::stream_protocol::endpoint("/run/controller/controller.sock"))
    {
        #ifdef DEBUG
        std::cout << "Unix Domain Socket Server Constructor!" << std::endl;
        #endif
    }

    Server::Server(boost::asio::io_context& ioc, boost::asio::local::stream_protocol::endpoint endpoint)
      : ioc_(ioc),
        acceptor_(ioc, endpoint)
    {
        #ifdef DEBUG
        std::cout << "Unix Domain Socket Server Endpoint Constructor!" << std::endl;
        #endif
    }

    Server::~Server(){
        #ifdef DEBUG
        std::cout << "Unix Socket Server Destructor" << std::endl;
        #endif
    }

    void Server::start_accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn){
        acceptor_.async_accept(fn);
    }

    void unix_session::async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn){
        socket_.async_read_some(
            boost::asio::buffer(buf().data(), unix_session::max_buflen),
            boost::asio::bind_cancellation_slot(
                stop_signal_.slot(),
                fn
            )
        );
    }

    void unix_session::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn){
            std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
            std::memcpy(write_data_ptr->data(), write_buffer.data(), write_data_ptr->size());
            boost::asio::const_buffer buf(write_data_ptr->data(), write_data_ptr->size());
            socket_.async_write_some(
                buf,
                [&, buf, write_data_ptr, fn](const boost::system::error_code& ec, std::size_t bytes_transferred){
                    if (!ec){
                        std::size_t remaining_bytes = write_data_ptr->size() - bytes_transferred;
                        if ( remaining_bytes > 0 ){
                            boost::asio::const_buffer buf = buf + bytes_transferred;
                            async_write(buf, fn);
                        } else {
                            // Once the write is complete execute the completion
                            // handler.
                            fn();
                        }
                    }
                }
            );
        }

    void unix_session::close(){
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_both, ec);
        socket_.close(ec);        
    }

    unix_server::unix_server(boost::asio::io_context& ioc)
      : server::Server(ioc),
        acceptor_(ioc)
    {}

    unix_server::unix_server(boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : server::Server(ioc),
        acceptor_(ioc, endpoint)
    {}

    void unix_server::accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn){
        acceptor_.async_accept(fn);
    }
}// Unix Server Namespace