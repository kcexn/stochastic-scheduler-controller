#include "unix-server.hpp"
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


    void Session::shutdown_read(){
        #ifdef DEBUG
        std::cout << "Shutdown the Read side of the Unix socket." << std::endl;
        #endif
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_receive);
    }

    void Session::shutdown_write(){
        #ifdef DEBUG
        std::cout << "Shutdown the write side of the Unix socket." << std::endl;
        #endif
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_send);
    }

    void Session::close(){
        #ifdef DEBUG
        std::cout << "Shutdown both sides of the Unix socket. Close the Unix socket." << std::endl;
        #endif
        socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_both);
        socket_.close();
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
        std::filesystem::remove("/run/controller/controller.sock");
    }

    void Server::start_accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn){
        acceptor_.async_accept(fn);
    }
}// Unix Server Namespace