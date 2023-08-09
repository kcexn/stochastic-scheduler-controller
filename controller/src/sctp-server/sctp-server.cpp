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
sctp_server::server::server(boost::asio::io_context& ioc, short port)
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
        perror("setsockopt SCTP_RECVRCVINFO failed.");
    }
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECVNXTINFO, &sctp_recvnxtinfo, sizeof(sctp_recvnxtinfo)) == -1){
        perror("setsockopt SCTP_RECVNXTINFO failed.");
    }
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECONFIG_SUPPORTED, &sctp_future_assoc, sizeof(sctp_future_assoc)) == -1){
        perror("setsockopt SCTP_RECONFIG_SUPPORTED failed.");
    }
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &sctp_reset_future_streams, sizeof(sctp_reset_future_streams)) == -1){
        perror("setsockopt SCTP_ENABLE_STREAM_RESET failed.");
    }
    if( listen(sockfd, 128) == -1){
        int errsv = errno;
        std::cerr << "Listen failed with errno: " << std::to_string(errsv) << std::endl;
    }
}

//Destructor
sctp_server::server::~server(){
    // Free SCTP server buffers.
    for(int i=0; i<num_bufs; ++i){
        free(bufs[i].iov_base);
    }
}


sctp::sctp_message sctp_server::server::do_read(){
    //SCTP provides additional message metadata in the msghdr struct that is returned with recvmsg().
    //This struct is important for retrieving SCTP transport layer information such as:
    //Association ID, Stream ID, and the Stream Sequence Number. Boost.Asio does not 
    //provide any native support for these message header metadata structures, and so 
    //we must escape to the native socket handle again. See man(7) sctp, man(3) cmsg, man(2) recvmsg, man(2) readv.
    int sockfd = socket_.native_handle();

    //Initialize an SCTP Control Message Buffer.
    char cbuf[cbuflen] = {};
    struct sockaddr_in sin;

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

        struct sctp_rcvinfo rcvinfo = {};
        for( sctp::message_controls cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg) ){
            if (cmsg->cmsg_level == IPPROTO_SCTP && cmsg->cmsg_type == SCTP_RCVINFO){
                std::memcpy(&rcvinfo, CMSG_DATA(cmsg), sizeof(rcvinfo));
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

        //Construct the SCTP Received Message.
        sctp::sctp_message rcv_msg = {
            .rmt_endpt = {
                .endpt = endpt,
                .rcvinfo = rcvinfo
            },
            .payload = payload
        };

        sctp_server::sctp_stream strm(rcv_msg.rmt_endpt.rcvinfo.rcv_assoc_id, rcv_msg.rmt_endpt.rcvinfo.rcv_sid);
        if (!sctp_server::server::is_existing_stream(strm)){
            stream_table.push_back(std::move(strm));
        }
                
        std::cout.write(static_cast<const char*>(rcv_msg.payload.data()), length) << std::flush;
        std::cout << rcv_msg.rmt_endpt.endpt << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_assoc_id) << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_flags) << std::endl;

        //Route and Handle the Incoming Messages.
        rcvdmsg = std::move(rcv_msg);
        return rcvdmsg;
    }
    throw "recvmsg has thrown an error.";
}

void sctp_server::server::do_write(const sctp::sctp_message& msg){
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

bool sctp_server::server::is_existing_stream(const sctp_server::sctp_stream& other_strm){
    for (auto strm: stream_table){
        if (strm == other_strm){
            return true;
        }
    }
    return false;
}