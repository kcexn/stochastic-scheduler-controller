#include "unix-server.hpp"
#include <filesystem>

//-------------------------------------|
//Unix-Server Sessions.
UnixServer::Session::Session(boost::asio::local::stream_protocol::socket socket)
    : socket_(std::move(socket)){}

void UnixServer::Session::async_read(
    std::function<void(boost::system::error_code ec, std::size_t length)> fn
    )
{
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        fn
    );
}

void UnixServer::Session::do_write(std::size_t length){
    socket_.write_some(boost::asio::buffer(data_, length));
}
//-----------------------------------------|
//Unix-Server Server
UnixServer::Server::Server(boost::asio::io_context& ioc)
  : ioc_(ioc),
    acceptor_(ioc, boost::asio::local::stream_protocol::endpoint("/run/controller/controller.sock"))
{
    #ifdef DEBUG
    std::cout << "Unix Domain Socket Server Constructor!" << std::endl;
    #endif
}

void UnixServer::Session::shutdown_read(){
    #ifdef DEBUG
    std::cout << "Shutdown the Read side of the Unix socket." << std::endl;
    #endif
    socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_receive);
}

void UnixServer::Session::shutdown_write(){
    #ifdef DEBUG
    std::cout << "Shutdown the write side of the Unix socket." << std::endl;
    #endif
    socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_send);
}

void UnixServer::Session::close(){
    #ifdef DEBUG
    std::cout << "Shutdown both sides of the Unix socket. Close the Unix socket." << std::endl;
    #endif
    socket_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_type::shutdown_both);
    socket_.close();
}

UnixServer::Server::~Server(){
    #ifdef DEBUG
    std::cout << "Unix Socket Server Destructor" << std::endl;
    #endif
    std::filesystem::remove("/run/controller/controller.sock");
}

void UnixServer::Server::start_accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn){
    acceptor_.async_accept(fn);
}
