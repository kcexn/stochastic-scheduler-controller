#include "sctp-server.hpp"
#include <cstdio>
#include <cerrno>

#ifdef DEBUG
#include <iostream>
#endif

//Constructor
sctp_server::server::server(boost::asio::io_context& ioc, short port)
    : socket_(ioc, echo::sctp::endpoint(echo::sctp::v4(), port)), ioc_(ioc)
{
    // Initialize SCTP message buffers.
    for (int i = 0; i < num_bufs; ++i){
        bufs[i].iov_base = static_cast<echo::sctp::buffer*>(malloc(buflen));
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
    if (setsockopt(sockfd, IPPROTO_SCTP, SCTP_AUTOCLOSE, &sctp_autoclose, sizeof(sctp_autoclose)) == -1){
        perror("setsockopt SCTP_ENABLE_STREAM_RESET failed.");
    }
    if( listen(sockfd, 128) == -1){
        perror("Listen failed.");
    }
}

//Destructor
sctp_server::server::~server(){
    #ifdef DEBUG
    std::cout << "SCTP Server Destructor!" << std::endl;
    #endif
    // Free SCTP server buffers.
    for(int i=0; i<num_bufs; ++i){
        free(bufs[i].iov_base);
    }
}


echo::sctp::sctp_message sctp_server::server::do_read(){
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
    echo::sctp::envelope msg = {
        &sin,
        sizeof(sin),
        bufs,
        num_bufs,
        cbuf,
        cbuflen,
        0
    };

    int length = recvmsg(sockfd, &msg, 0);
    if (length == -1){
        perror("recvmsg failed");
    } else {
        boost::asio::const_buffer payload(bufs[0].iov_base, length);

        struct sctp_rcvinfo rcvinfo = {};
        for( echo::sctp::message_controls cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg) ){
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
        echo::sctp::endpoint endpt( addr, port );

        //Construct the SCTP Received Message.
        echo::sctp::sctp_message rcv_msg = {
            {
                endpt,
                rcvinfo
            },
            payload
        };

        // sctp_server::sctp_stream strm(rcv_msg.rmt_endpt.rcvinfo.rcv_assoc_id, rcv_msg.rmt_endpt.rcvinfo.rcv_sid);
        // if (!sctp_server::server::is_existing_stream(strm)){
        //     stream_table.push_back(std::move(strm));
        // }
        #ifdef DEBUG
        std::cout.write(static_cast<const char*>(rcv_msg.payload.data()), length) << std::flush;
        std::cout << rcv_msg.rmt_endpt.endpt << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_assoc_id) << std::endl;
        std::cout << std::to_string(rcv_msg.rmt_endpt.rcvinfo.rcv_flags) << std::endl;
        #endif

        //Route and Handle the Incoming Messages.
        rcvdmsg = std::move(rcv_msg);
        return rcvdmsg;
    }
    throw "recvmsg has thrown an error.";
}

void sctp_server::server::do_write(const echo::sctp::sctp_message& msg){
    //SCTP provides additional message metadata in the msghdr struct that is returned with recvmsg().
    //This struct is important for retrieving SCTP transport layer information such as:
    //Association ID, Stream ID, and the Stream Sequence Number. Boost.Asio does not 
    //provide any native support for these message header metadata structures, and so 
    //we must escape to the native socket handle again. See man(7) sctp, man(3) cmsg, man(2) sendmsg, man(2) writev.
    int sockfd = socket_.native_handle();

    #ifdef DEBUG
    std::cout << "SCTP Write out: ";
    std::cout.write(static_cast<const char*>(msg.payload.data()), msg.payload.size());
    std::cout << std::endl;
    #endif

    //Initialize a buffer for ancillary data.
    char cbuf[CMSG_SPACE(sizeof(echo::sctp::sndinfo))] = {};

    //Initialize payload buffers for writing to the socket.
    std::vector<char> pbuf(msg.payload.size());
    std::memcpy(pbuf.data(), msg.payload.data(), msg.payload.size());
    echo::sctp::buffer pbufs[1] = {
        {
            pbuf.data(),
            pbuf.size()          
        }
    };

    //Initialize a message envelope
    echo::sctp::envelope sndmsg = {
        NULL,
        0,
        pbufs,
        num_bufs,
        cbuf,
        sizeof(cbuf),
        0
    };

    echo::sctp::message_controls snd_cmsg = CMSG_FIRSTHDR(&sndmsg);
    if ( snd_cmsg == NULL ){
        perror("cmesg buffer error.");
    } else {
        snd_cmsg->cmsg_level = IPPROTO_SCTP;
        snd_cmsg->cmsg_type = SCTP_SNDINFO;
        snd_cmsg->cmsg_len = CMSG_LEN(sizeof(echo::sctp::sndinfo));
        std::memcpy(CMSG_DATA(snd_cmsg), &msg.rmt_endpt.sndinfo, sizeof(echo::sctp::sndinfo));
    }

    if(sendmsg(sockfd, &sndmsg, MSG_NOSIGNAL) == -1){
        perror("send message failed.");
    }
}

void sctp_server::server::async_read(std::function<void(const boost::system::error_code& ec)>&& f){
    socket_.async_wait(
        echo::sctp::socket::wait_read,
        f
    );
}

void sctp_server::server::shutdown_read(echo::sctp::endpoint remote, echo::sctp::assoc_t assoc_id_){
    // TODO: Strictly speaking, this implementation of SCTP SHUTDOWN should use the 
    // kernel association address table which can be retrieved with a call to getsockopt,
    // and not the application address table.
    //
    // This is because SCTP keeps track of association availability using SCTP HEARTBEAT messages.
    // An underlying peer can "move" its IP address with a peer address change update.
    //
    // Since the available IP addresses of the association may have changed between the time
    // a message has arrived, and the time the SHUTDOWN message is sent, the current 
    // active IP address should be retrieved from the kernel table, rather than from 
    // the application table.
    int sockfd = socket_.native_handle();

    // Construct the sndinfo.
    echo::sctp::sndinfo shutdown = {
        0,
        SCTP_EOF,
        0,
        0,
        assoc_id_
    };

    //Initialize a buffer for ancillary data.
    char cbuf[CMSG_SPACE(sizeof(echo::sctp::sndinfo))] = {};

    struct in_addr addr_v4 = {};
    inet_pton(AF_INET, remote.address().to_string().c_str(), &addr_v4);

    struct sockaddr_in addr = {
        AF_INET,
        htons(remote.port()),
        addr_v4
    };

    //Initialize a message envelope
    echo::sctp::envelope sndmsg = {
        &addr,
        sizeof(addr),
        nullptr,
        0,
        cbuf,
        sizeof(cbuf),
        0
    };


    echo::sctp::message_controls snd_cmsg = CMSG_FIRSTHDR(&sndmsg);
    if ( snd_cmsg == NULL ){
        perror("cmesg buffer error.");
    } else {
        snd_cmsg->cmsg_level = IPPROTO_SCTP;
        snd_cmsg->cmsg_type = SCTP_SNDINFO;
        snd_cmsg->cmsg_len = CMSG_LEN(sizeof(echo::sctp::sndinfo));
        std::memcpy(CMSG_DATA(snd_cmsg), &shutdown, sizeof(echo::sctp::sndinfo));
    }

    if(sendmsg(sockfd, &sndmsg, 0) == -1){
        perror("shutdown failed.");
    }

    #ifdef DEBUG
    std::cout << "Association Shutdown Successful!" << std::endl;
    #endif
}

void sctp_server::server::stop(){
    socket_.close();
}

void sctp_server::server::start(){
    ioc_.run();
}