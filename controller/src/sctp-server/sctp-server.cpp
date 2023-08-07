#include <iostream>
#include "sctp.hpp"
#include "sctp-server.hpp"

#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <errno.h>

sctp_server::sctp_server(boost::asio::io_context& ioc, short port)
    : socket_(ioc, sctp::endpoint(sctp::v4(), port))
{
    //SCTP is message based and streams are multiplexed so it doesn't need to accept and return a new socket file descriptr. 
    //This is in strong contrast to TCP. Boost.Asio doesn't support listen but not accept semantics, so we need
    //to escape to the native socket handle and call listen manually.
    int sockfd = socket_.native_handle();
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &sctp_recvrcvinfo, sizeof(sctp_recvrcvinfo)) == -1){
        int errsv = errno;
        std::cout << "setsockopt SCTP_RECVRCVINFO, failed with errno: " << std::to_string(errsv) << std::endl;
    }
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECVNXTINFO, &sctp_recvnxtinfo, sizeof(sctp_recvnxtinfo)) == -1){
        int errsv = errno;
        std::cout << "setsockopt SCTP_RECVNXTINFO, failed with errno: " << std::to_string(errsv) << std::endl;
    }
    if( listen(sockfd, 128) == -1){
        int errsv = errno;
        std::cout << "Listen failed with errno: " << std::to_string(errsv) << std::endl;
    }
    do_read();
}

void sctp_server::do_read(){
    //SCTP provides additional message metadata in the msghdr struct that is returned with recvmsg().
    //This struct is important for retrieving SCTP transport layer information such as:
    //Association ID, Stream ID, and the Stream Sequence Number. Boost.Asio does not 
    //provide any native support for these message header metadata structures, and so 
    //we must escape to the native socket handle again. See man(7) sctp, man(3) cmsg, man(2) recvmsg, man(2) readv.
    int sockfd = socket_.native_handle();

    struct iovec msg_iov[1];
    //Initialize the control message with enough space on the stack for the various ancillary messages
    //that SCTP can receive in recvmsg.
    char cmsg[sizeof(sctp_rcvinfo) + sizeof(sctp_nxtinfo) + sizeof(sctp_sndrcvinfo) + sizeof(cmsghdr)];
    struct sockaddr_in sin = {};
    msg_iov[0].iov_base = data_;
    msg_iov[0].iov_len = max_length;

    //All Properties in the struct need to be properly initialized before recvmsg will work for SCTP.
    struct msghdr msgh;
    msgh.msg_iov = msg_iov;
    msgh.msg_iovlen = 1;
    msgh.msg_name = &sin;
    msgh.msg_namelen = sizeof(sin);
    msgh.msg_control = &cmsg;
    msgh.msg_controllen = sizeof(cmsg);

    int length = recvmsg(sockfd, &msgh, 0);
    if (length == -1){
        perror("recvmsg failed");
    } else {
        boost::asio::const_buffer buf(data_, length);
        std::cout.write(reinterpret_cast<const char*>(buf.data()), length) << std::flush;
    }
}