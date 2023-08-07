#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP

#include <boost/asio.hpp>
#include "sctp.hpp"
#include "sctp-server.hpp"

class sctp_server
{
public:
    sctp_server(boost::asio::io_context& ioc, short port);
    ~sctp_server() = default;
private:
    void do_read();
    enum { max_length = 1024 };
    char data_[max_length];

    sctp::socket socket_;

    int sctp_recvrcvinfo = 1;
    int sctp_recvnxtinfo = 1;
};

#endif //SCTP_SERVER_HPP