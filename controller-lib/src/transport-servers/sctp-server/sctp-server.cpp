#include "sctp-server.hpp"
#include "sctp-session.hpp"
#include <cerrno>

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

        struct sctp_event subscribe = {
            SCTP_ALL_ASSOC,
            SCTP_ASSOC_CHANGE,
            1
        };
        int val = setsockopt(acceptor.native_handle(), IPPROTO_SCTP, SCTP_EVENT, &subscribe, sizeof(subscribe));


        acceptor.listen();
        int sockfd = acceptor.native_handle();
        acceptor.release();
        socket_.assign(transport::protocols::sctp::v4(), sockfd);
    }

    void SctpServer::init(std::function<void(const boost::system::error_code&, std::shared_ptr<SctpSession>)> fn){
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_read,
            std::bind(&SctpServer::read, this, fn, std::placeholders::_1)
        );
    }

    void SctpServer::stop(){
        acquire();
        clear();
        release();
    }

    void SctpServer::async_connect(server::Remote rmt, std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)> fn){
        std::shared_ptr<SctpSession> session;
        /* Get assoc params from the socket. */
        transport::protocols::sctp::paddrinfo paddrinfo = {};
        socklen_t paddrinfo_size = sizeof(paddrinfo);
        std::memcpy(&(paddrinfo.spinfo_address), &(rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
        int ec = getsockopt(socket_.native_handle(), IPPROTO_SCTP, SCTP_GET_PEER_ADDR_INFO, &paddrinfo, &paddrinfo_size);
        if(ec == -1){
            if(errno == EINVAL){
                /* remote address is not in the peer address table. */
                transport::protocols::sctp::stream_t stream = {
                    SCTP_FUTURE_ASSOC,
                    0
                };
                session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
                acquire();
                pending_connects_.emplace_back();
                {
                    auto& pending_connect = pending_connects_.back();
                    pending_connect.session = session;
                    pending_connect.cb = fn;
                }
                release();
                socket_.async_wait(
                    transport::protocols::sctp::socket::wait_type::wait_write,
                    [fn, rmt, this, session](const boost::system::error_code& ec) mutable {
                        if(!ec){
                            int err = connect(this->socket_.native_handle(), (const struct sockaddr*)(&rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                            boost::system::error_code error;
                            if(errno != EINPROGRESS){
                                error = boost::system::error_code(errno, boost::system::system_category());
                                this->erase_pending_connect(session);
                                fn(error, session);
                            } else {
                                transport::protocols::sctp::paddrinfo paddr;
                                socklen_t paddr_size = sizeof(paddr);
                                std::memcpy(&(paddr.spinfo_address), &(rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                                int ec = getsockopt(this->socket_.native_handle(), IPPROTO_SCTP, SCTP_GET_PEER_ADDR_INFO, &paddr, &paddr_size);
                                if(ec == -1){
                                    perror("getsockopt failed");
                                    throw "This really shouldn't happen.";
                                }
                                transport::protocols::sctp::assoc_t assoc_id = paddr.spinfo_assoc_id;
                                session->set(assoc_id);
                            }
                        }
                        return;
                    }
                );
            } else {
                perror("getsockopt failed");
            }
        } else {
            /* Remote address is in the peer address table. */
            transport::protocols::sctp::assoc_t assoc_id = paddrinfo.spinfo_assoc_id;
            /* Look for the next available stream*/
            transport::protocols::sctp::sid_t last_stream_no = 0;
            acquire();
            for(auto& ptr: *this){
                if(std::static_pointer_cast<SctpSession>(ptr)->assoc() == assoc_id){
                    if( (std::static_pointer_cast<SctpSession>(ptr)->sid() - last_stream_no) <= 1){
                        last_stream_no = std::static_pointer_cast<SctpSession>(ptr)->sid();
                    } else {
                        break;
                    }
                }
            }
            ++last_stream_no;
            transport::protocols::sctp::stream_t stream = {
                assoc_id,
                last_stream_no
            };
            session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
            push_back(session);
            release();
            boost::system::error_code ec;
            fn(ec, session);
        }
        return;
    }

    void SctpServer::erase_pending_connect(const std::shared_ptr<server::Session>& sctp_session)
    { 
        acquire();
        auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
            return pending_connection.session == sctp_session;
        });
        if(it != pending_connects_.end()){
            pending_connects_.erase(it);
        }
        release();
        return;
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
                        transport::protocols::sctp::assoc_t association = sac->sac_assoc_id;
                        switch(sac->sac_state)
                        {
                            case SCTP_COMM_UP:
                            {
                                /* Search for the association in the pending connects table */
                                acquire();
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    return pending_connection.session->get_assoc() == association;
                                });
                                if(it != pending_connects_.end()){
                                    /* This is a pending connection */
                                    boost::system::error_code error;
                                    const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                    push_back(sctp_session);
                                    pending_connects_.erase(it);
                                    it->cb(error, sctp_session);
                                }
                                release();
                                /* Otherwise it's a brand new incoming connection. */
                                break;
                            }
                            case SCTP_COMM_LOST:
                                break;
                            case SCTP_RESTART:
                                break;
                            case SCTP_SHUTDOWN_COMP:
                                break;
                            case SCTP_CANT_STR_ASSOC:
                            {
                                /* Search for the association in the pending connects table */
                                acquire();
                                auto it = std::find_if(pending_connects_.cbegin(), pending_connects_.cend(), [&](const auto& pending_connection){
                                    return pending_connection.session->get_assoc() == association;
                                });
                                if(it != pending_connects_.cend()){
                                    /* This is a pending connection */
                                    boost::system::error_code error(ECONNREFUSED, boost::system::system_category());
                                    const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                    it->cb(error, it->session);
                                    pending_connects_.erase(it);
                                }
                                release();
                                break;
                            }
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
                } else {
                    sctp_session = std::static_pointer_cast<sctp_transport::SctpSession>(*it);
                }
                std::string received_data(buf_.data(), len);
                sctp_session->read(ec, received_data);

                // Call the read function callback.
                fn(ec, sctp_session);
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