#include "sctp-server.hpp"
#include "sctp-session.hpp"

#ifdef DEBUG
#include <iostream>
#endif

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

        // struct sctp_event subscribe = {
        //     SCTP_ALL_ASSOC,
        //     SCTP_ASSOC_CHANGE,
        //     1
        // };
        // int val = setsockopt(acceptor.native_handle(), IPPROTO_SCTP, SCTP_EVENT, &subscribe, sizeof(subscribe));


        acceptor.listen();
        int sockfd = acceptor.native_handle();
        acceptor.release();
        socket_.assign(transport::protocols::sctp::v4(), sockfd);
    }

    void SctpServer::init(std::function<void(const boost::system::error_code&, std::shared_ptr<sctp_transport::SctpSession>)> fn){
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_read,
            std::bind(&SctpServer::read, this, fn, std::placeholders::_1)
        );
    }

    void SctpServer::stop(){
        clear();
    }

    void SctpServer::read(std::function<void(const boost::system::error_code&, std::shared_ptr<sctp_transport::SctpSession>)> fn, const boost::system::error_code& ec){
        if(!ec){
            using namespace transport::protocols;
            sctp::iov iobuf= {
                buf_.data(),
                buf_.size()
            };
            sctp::sockaddr_in addr = {};
            sctp::rcvinfo rcvinfo = {};
            sctp::msghdr msg = {
                &addr,
                sizeof(sctp::sockaddr_in),
                &iobuf,
                1,
                cbuf_.data(),
                cbuf_.size(),
                0                                     
            };
            int len = recvmsg(socket_.native_handle(), &msg, 0);
            if( len == -1 ){
                perror("recvmsg failed");
            }
            if(msg.msg_flags & MSG_NOTIFICATION){
                union sctp_notification* snp;
                snp = (sctp_notification*)(buf_.data());
                switch(snp->sn_header.sn_type)
                {
                    case SCTP_ASSOC_CHANGE:
                        struct sctp_assoc_change* sac;
                        sac = &snp->sn_assoc_change;
                        switch(sac->sac_state)
                        {
                            case SCTP_COMM_UP:
                                // std::cout << sac->sac_inbound_streams << std::endl;
                                break;
                            case SCTP_COMM_LOST:
                                break;
                            case SCTP_RESTART:
                                break;
                            case SCTP_SHUTDOWN_COMP:
                                break;
                            case SCTP_CANT_STR_ASSOC:
                                break;
                            default:
                                break;
                        }
                        break;
                }
            } else {
                sctp::cmsghdr* cmsg;
                for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)){
                    if(cmsg->cmsg_len == 0){
                        throw "cmsg_len should never be 0.";
                    }
                    if(cmsg->cmsg_level == sctp::v4().protocol() && SCTP_RCVINFO){
                        std::memcpy(&rcvinfo, CMSG_DATA(cmsg), sizeof(sctp::rcvinfo));
                        break;
                    }
                }
                sctp::stream_t stream_id = {
                    rcvinfo.rcv_assoc_id,
                    rcvinfo.rcv_sid
                };
            
                acquire();
                auto it = std::find_if(cbegin(), cend(), [&](auto& ptr){
                    return *(std::static_pointer_cast<sctp_transport::SctpSession>(ptr)) == stream_id;
                });
                release();
                std::shared_ptr<sctp_transport::SctpSession> sctp_session;
                if(it == cend()){
                    // Create a new session.
                    sctp_session = std::make_shared<sctp_transport::SctpSession>(*this, stream_id, socket_);
                    push_back(sctp_session);
                    // Accept and return a new context (similar to the berkeley sockets accept call.)
                    fn(ec, sctp_session);
                } else {
                    sctp_session = std::static_pointer_cast<sctp_transport::SctpSession>(*it);
                }
                std::string received_data(buf_.data(), len);
                sctp_session->read(ec, received_data);
            }
            socket_.async_wait(
                transport::protocols::sctp::socket::wait_type::wait_read,
                std::bind(&SctpServer::read, this, fn, std::placeholders::_1)
            );
        } else {
            std::shared_ptr<sctp_transport::SctpSession> empty_session;
            fn(ec, empty_session);
        }
    }

    SctpServer::~SctpServer(){
        stop();
        socket_.close();
    }
}