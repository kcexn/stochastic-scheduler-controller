#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP

#include <boost/asio.hpp>
#include <sys/uio.h>

#include "sctp.hpp"

class sctp_server
{
public:
    sctp_server(boost::asio::io_context& ioc, short port);
    ~sctp_server();
private:
    void do_read();
    void do_write(const sctp::sctp_message& msg);
    enum { 
        buflen = 1024, /* each data buffer will be of length buflen */
        cbuflen = 1024, /* the control message buffer will be of length cbuflen, 1024 should be enough for even very large numbers of control headers.*/
        num_bufs = 1
    };
    sctp::socket socket_;
    sctp::buffer bufs[num_bufs];
    
    // read flags.
    int sctp_recvrcvinfo = 1;
    int sctp_recvnxtinfo = 1;
};

#endif //SCTP_SERVER_HPP