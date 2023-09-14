#include "sctp-server.hpp"

namespace sctp_transport{
    SctpServer::SctpServer(boost::asio::io_context& ioc)
      : server::Server(ioc),
        socket_(ioc)
    {
        buf_.fill(0);
        cbuf_.fill(0);
    }

    SctpServer::SctpServer(boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
      : server::Server(ioc),
        socket_(ioc)
    {
        buf_.fill(0);
        cbuf_.fill(0);
        transport::protocols::sctp::acceptor acceptor(ioc, transport::protocols::sctp::v4());
        acceptor.bind(endpoint);
        transport::protocols::sctp::recvrcvinfo option(1);
        acceptor.set_option(option);
        acceptor.listen();
        int sockfd = acceptor.native_handle();
        acceptor.release();
        socket_.assign(transport::protocols::sctp::v4(), sockfd);
    }

    void SctpServer::init(std::function<void(const boost::system::error_code& ec)> fn){
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_read,
            std::bind(&SctpServer::read, this, fn, std::placeholders::_1)
        );
    }

    void SctpServer::stop(){
        clear();
    }

    void SctpServer::read(std::function<void(const boost::system::error_code& ec)> fn, const boost::system::error_code& ec){
        if(!ec){
            using namespace transport::protocols;
            sctp::iov iobuf= {
                buf_.data(),
                buf_.size()
            };
            sctp::msghdr msg = {
                nullptr,
                0,
                &iobuf,
                1,
                cbuf_.data(),
                cbuf_.size(),
                0                                     
            };
            std::size_t len = recvmsg(socket_.native_handle(), &msg, 0);
            std::string received_data(buf_.data(), len);
            fn(ec);
            socket_.async_wait(
                transport::protocols::sctp::socket::wait_type::wait_read,
                std::bind(&SctpServer::read, this, fn, std::placeholders::_1)
            );
        } else {
            fn(ec);
        }
    }

    SctpServer::~SctpServer(){
        stop();
        socket_.close();
    }
}