#include "unix-server.hpp"

//-------------------------------------|
//Unix-Server Sessions.
UnixServer::Session::Session(boost::asio::local::stream_protocol::socket socket)
    : socket_(std::move(socket)){}

void UnixServer::Session::start(){
    do_read();
}

void UnixServer::Session::do_read(){
    std::shared_ptr<Session> self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length){
            if (!ec){
                do_write(length);
            }
        }
    );
}

void UnixServer::Session::do_write(std::size_t length){
    std::shared_ptr<Session> self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/){
            if (!ec){
                do_read();
            }
        }
    );
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
    start_accept();
}

void UnixServer::Server::start_accept(){
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::local::stream_protocol::socket socket)
        {
            if (!ec){
                std::make_shared<Session>(std::move(socket))->start();
            }
            start_accept();
        }
    );
}
