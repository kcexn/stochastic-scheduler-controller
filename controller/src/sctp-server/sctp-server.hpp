#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP

#include <boost/asio.hpp>
#include <iostream>
#include "sctp.hpp"
#include "sctp-server.hpp"

class sctp_server
{
public:
    sctp_server(boost::asio::io_context& ioc, short port);
private:
    sctp::socket socket_;
    sctp::endpoint sender_endpoint_;
    enum { max_length = 1024 };
    char data_[max_length];
};

#endif //SCTP_SERVER_HPP