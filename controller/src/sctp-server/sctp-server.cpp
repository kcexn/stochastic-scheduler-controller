#include <iostream>
#include <boost/asio.hpp>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <errno.h>

#include "sctp.hpp"
#include "sctp-server.hpp"

//Constructor
sctp_server::sctp_server(boost::asio::io_context& ioc, short port)
    : socket_(ioc, sctp::endpoint(sctp::v4(), port))
{
    // Initialize SCTP message buffers.
    for (int i = 0; i < num_bufs; ++i){
        bufs[i].iov_base = static_cast<sctp::buffer*>(malloc(buflen));
        bufs[i].iov_len = buflen;
    }

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

//Destructor
sctp_server::~sctp_server(){
    // Free SCTP server buffers.
    for(int i=0; i<num_bufs; ++i){
        free(bufs[i].iov_base);
    }
}


void sctp_server::do_read(){
    //SCTP provides additional message metadata in the msghdr struct that is returned with recvmsg().
    //This struct is important for retrieving SCTP transport layer information such as:
    //Association ID, Stream ID, and the Stream Sequence Number. Boost.Asio does not 
    //provide any native support for these message header metadata structures, and so 
    //we must escape to the native socket handle again. See man(7) sctp, man(3) cmsg, man(2) recvmsg, man(2) readv.
    int sockfd = socket_.native_handle();

    struct sockaddr_in sin;

    //Initialize an SCTP Control Message Buffer.
    sctp::cbuf cbuf = malloc(cbuflen);

    //All Properties in the struct need to be properly initialized before recvmsg will work for SCTP.

    //Initialize a message envelope
    sctp::envelope msg = {
        .msg_name = &sin,
        .msg_namelen = sizeof(sin),
        .msg_iov = bufs,
        .msg_iovlen = num_bufs,
        .msg_control = cbuf,
        .msg_controllen = cbuflen,
        .msg_flags = 0
    };

    int length = recvmsg(sockfd, &msg, 0);
    if (length == -1){
        perror("recvmsg failed");
    } else {
        boost::asio::const_buffer payload(bufs[0].iov_base, length);

        struct sctp_rcvinfo rcvinfo;
        for( sctp::message_controls cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg) ){
            if (cmsg->cmsg_level == IPPROTO_SCTP && cmsg->cmsg_type == SCTP_RCVINFO){
                memcpy(&rcvinfo, CMSG_DATA(cmsg), sizeof(rcvinfo));
                break;
            }
        }
        char dst[16];
        const char* addr_string = inet_ntop(AF_INET, &sin.sin_addr, dst, 16);
        uint16_t port = ntohs(sin.sin_port);
        boost::asio::ip::address addr = boost::asio::ip::make_address(
            addr_string
        );
        sctp::endpoint endpt( addr, port );

        //Construct the SCTP Remote Endpoint.
        sctp::sctp_rmt_endpt rmt_endpt = {
            .endpt = endpt,
            .rcvinfo = rcvinfo
        };

        //Construct the SCTP Received Message.
        sctp::sctp_message rcv_msg = {
            .rmt_endpt = rmt_endpt,
            .payload = payload
        };

        std::cout.write(static_cast<const char*>(rcv_msg.payload.data()), length) << std::flush;
        std::cout << rcv_msg.rmt_endpt.endpt << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_assoc_id) << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_flags) << std::endl;

        //Echo Server.
        sctp::sndinfo sndinfo = {
            .snd_sid = 0,
            .snd_flags = 0,
            .snd_ppid = 0,
            .snd_context = 0,
            .snd_assoc_id = rmt_endpt.rcvinfo.rcv_assoc_id
        };

        sctp::sctp_rmt_endpt dst_endpt = {
            .endpt = rmt_endpt.endpt,
            .sndinfo = sndinfo
        };


        sctp::sctp_message snd_msg = {
            .rmt_endpt = dst_endpt,
            .payload = payload
        };

        do_write(snd_msg);
        //Route and Handle the Incoming Messages.
    }
    free(cbuf);
    do_read();
}

void sctp_server::do_write(const sctp::sctp_message& msg){
    //SCTP provides additional message metadata in the msghdr struct that is returned with recvmsg().
    //This struct is important for retrieving SCTP transport layer information such as:
    //Association ID, Stream ID, and the Stream Sequence Number. Boost.Asio does not 
    //provide any native support for these message header metadata structures, and so 
    //we must escape to the native socket handle again. See man(7) sctp, man(3) cmsg, man(2) sendmsg, man(2) writev.
    int sockfd = socket_.native_handle();

    //Initialize a buffer for ancillary data.
    char cbuf[CMSG_SPACE(sizeof(sctp::sndinfo))] = {};

    //Initialize payload buffers for writing to the socket.
    char pbuf[msg.payload.size()] = {};
    std::memcpy(pbuf, msg.payload.data(), msg.payload.size());
    sctp::buffer pbufs[1] = {
        [0] = {
            .iov_base = pbuf,
            .iov_len = sizeof(pbuf)            
        }
    };

    //Initialize a message envelope
    sctp::envelope sndmsg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = pbufs,
        .msg_iovlen = num_bufs,
        .msg_control = cbuf,
        .msg_controllen = sizeof(cbuf),
        .msg_flags=0
    };

    sctp::message_controls snd_cmsg = CMSG_FIRSTHDR(&sndmsg);
    if ( snd_cmsg == NULL ){
        std::cerr << "cmesg buffer error." << std::endl;
    } else {
        snd_cmsg->cmsg_level = IPPROTO_SCTP;
        snd_cmsg->cmsg_type = SCTP_SNDINFO;
        snd_cmsg->cmsg_len = CMSG_LEN(sizeof(sctp::sndinfo));
        std::memcpy(CMSG_DATA(snd_cmsg), &msg.rmt_endpt.sndinfo, sizeof(sctp::sndinfo));
    }

    if(sendmsg(sockfd, &sndmsg, 0) == -1){
        perror("send message failed.");
    }
}