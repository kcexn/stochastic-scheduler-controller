#include "sctp-server.hpp"
#include "sctp-session.hpp"
#include <cerrno>
#include <iostream>

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
        int sockfd = socket(AF_INET, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_SCTP);
        if(sockfd == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:25:sctp_server failed to open:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        int recvrcvinfo = 1;
        int status = setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &recvrcvinfo, sizeof(recvrcvinfo));
        if(status == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:36:SCTP_RECVRCVINFO socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        struct sctp_event subscribe = {
            SCTP_ALL_ASSOC,
            SCTP_ASSOC_CHANGE,
            1
        };
        status = setsockopt(sockfd, IPPROTO_SCTP, SCTP_EVENT, &subscribe, sizeof(subscribe));
        if(status == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:51:SCTP_EVENT socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        int autoclose = 60;
        status = setsockopt(sockfd, IPPROTO_SCTP, SCTP_AUTOCLOSE, &autoclose, sizeof(autoclose));
        if(status == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:62:SCTP_AUTOCLOSE socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        struct sockaddr_in local_address = {};
        local_address.sin_family = AF_INET;
        local_address.sin_port = htons(endpoint.port());
        local_address.sin_addr.s_addr = htonl(endpoint.address().to_v4().to_uint());
        status = bind(sockfd, (const struct sockaddr*)(&local_address), sizeof(local_address));
        if(status == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:78:bind failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        status = listen(sockfd, 256);
        if(status == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:87:listen failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        socket_.assign(transport::protocols::sctp::v4(), sockfd);
    }

    void SctpServer::init(std::function<void(const boost::system::error_code&, std::shared_ptr<SctpSession>)> fn){
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_read,
            [&, fn](const boost::system::error_code& ec){
                read(fn, ec);
            }
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
                    1
                };
                session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
                PendingConnect connection = {session, fn};
                std::memcpy(&connection.addr, (const struct sockaddr*)(&rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                acquire();
                pending_connects_.push_back(connection);
                release();
                socket_.async_wait(
                    transport::protocols::sctp::socket::wait_type::wait_write,
                    [&, session, rmt, fn](const boost::system::error_code& ec) {
                        if(!ec){
                            int err = connect(socket_.native_handle(), (const struct sockaddr*)(&rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                            boost::system::error_code error;
                            if(err < 0 && errno != EINPROGRESS){
                                std::cerr << "sctp-server.cpp:131:connect failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                error = boost::system::error_code(errno, boost::system::system_category());
                                fn(error, session);
                                erase_pending_connect(session);
                            }
                            return;
                        } else {
                            std::cerr << "sctp-server.cpp:138:async_wait write failed:" << ec.message() << std::endl;
                            fn(ec, session);
                            erase_pending_connect(session);
                        }
                        return;
                    }
                );
            } else {
                std::cerr << "sctp-server.cpp:94:getsockopt failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            }
        } else {
            /* Remote address is in the peer address table. */
            transport::protocols::sctp::assoc_t assoc_id = paddrinfo.spinfo_assoc_id;
            /* Look for the next available stream*/
            std::vector<transport::protocols::sctp::sid_t> used_stream_nums;
            acquire();
            for(auto& session_ptr: *this){
                if(std::static_pointer_cast<SctpSession>(session_ptr)->assoc() == assoc_id){
                    used_stream_nums.push_back(std::static_pointer_cast<SctpSession>(session_ptr)->sid());
                }
            }
            release();
            std::sort(used_stream_nums.begin(), used_stream_nums.end());
            transport::protocols::sctp::sid_t last_stream_num = 0;
            for(auto sid: used_stream_nums){
                if(sid == last_stream_num){
                    ++last_stream_num;
                } else {
                    break;
                }
            }
            transport::protocols::sctp::stream_t stream = {
                assoc_id,
                last_stream_num
            };
            session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
            acquire();
            push_back(session);
            release();
            boost::system::error_code ec;
            fn(ec, session);
        }
        return;
    }

    void SctpServer::erase_pending_connect(std::shared_ptr<server::Session> sctp_session)
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
        // std::cerr << "sctp-server.cpp:197:SCTP READ EVENT" << std::endl;
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
            int len = 0;
            len = recvmsg(socket_.native_handle(), &msg, MSG_DONTWAIT);
            if(len == -1){
                switch(errno)
                {
                    case EWOULDBLOCK:
                        socket_.async_wait(
                            transport::protocols::sctp::socket::wait_type::wait_read,
                            [&, fn](const boost::system::error_code& ec){
                                read(fn, ec);
                            }
                        );
                        return;
                    case EINTR:
                        std::cerr << "sctp-server.cpp:228:recvmsg failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        socket_.async_wait(
                            transport::protocols::sctp::socket::wait_type::wait_read,
                            [&, fn](const boost::system::error_code& ec){
                                read(fn, ec);
                            }
                        );
                        return;
                    default:
                        std::cerr << "sctp-server.cpp:237:recvmsg failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "what?";
                }
            }
            if(msg.msg_flags & MSG_NOTIFICATION){
                union sctp_notification* snp;
                snp = (sctp_notification*)(buf_.data());
                switch(snp->sn_header.sn_type)
                {
                    case SCTP_ASSOC_CHANGE:
                    {
                        struct sctp_assoc_change* sac;
                        sac = &snp->sn_assoc_change;
                        transport::protocols::sctp::assoc_t association = sac->sac_assoc_id;
                        switch(sac->sac_state)
                        {
                            case SCTP_COMM_UP:
                            {
                                // std::cerr << "sctp-server.cpp:274:SCTP_COMM_UP EVENT" << std::endl;
                                /* Search for the association in the pending connects table */
                                boost::system::error_code error;
                                acquire();
                                try{
                                    auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                        sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                        return paddr->sin_addr.s_addr == addr.sin_addr.s_addr;
                                    });
                                    if(it != pending_connects_.end()){
                                        /* This is a pending connection */
                                        const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                        sctp_session->set(association);
                                        push_back(sctp_session);
                                        it->cb(error, sctp_session);
                                        pending_connects_.erase(it);
                                    }
                                } catch (std::bad_weak_ptr& e){
                                    std::cerr << "sctp-server.cpp:275:std::bad_weak_ptr thrown:" << e.what() << std::endl;
                                    throw e;
                                }
                                release();
                                /* Otherwise it's a brand new incoming connection. */
                                break;
                            }
                            case SCTP_COMM_LOST:
                                std::cerr << "sctp-server.cpp:274:SCTP_COMM_LOST EVENT" << std::endl;
                                break;
                            case SCTP_RESTART:
                                std::cerr << "sctp-server.cpp:277:SCTP_RESTART EVENT" << std::endl;
                                break;
                            case SCTP_SHUTDOWN_COMP:
                            {
                                // struct timespec ts = {};
                                // clock_gettime(CLOCK_REALTIME, &ts);
                                // std::cerr << "sctp-server.cpp:286:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":SCTP_SHUTDOWN_COMP EVENT" << std::endl;
                                boost::system::error_code error;
                                acquire();
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return paddr->sin_addr.s_addr == addr.sin_addr.s_addr;
                                });
                                if(it != pending_connects_.end()){
                                    /* This is a pending connection */
                                    pending_connects_.erase(it);
                                }
                                release();
                                break;
                            }
                            case SCTP_CANT_STR_ASSOC:
                            {
                                std::cerr << "sctp-server.cpp:284:SCTP_CANT_STR_ASSOC EVENT" << std::endl;
                                /* Search for the association in the pending connects table */
                                acquire();
                                auto it = std::find_if(pending_connects_.cbegin(), pending_connects_.cend(), [&](const auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return paddr->sin_addr.s_addr == addr.sin_addr.s_addr;
                                });
                                if(it != pending_connects_.cend()){
                                    /* This is a pending connection */
                                    boost::system::error_code error(ECONNREFUSED, boost::system::system_category());
                                    const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                    it->cb(error, sctp_session);
                                    pending_connects_.erase(it);
                                }
                                release();
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        std::cerr << "sctp-server.cpp:320:SCTP UNRECOGNIZED EVENT:" << snp->sn_header.sn_type << std::endl;
                        break;
                }
            } else if (len > 0) {
                sctp::cmsghdr* cmsg;
                for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)){
                    if(cmsg->cmsg_len == 0){
                        std::cerr << "sctp-server.cpp:243:cmsg_len == 0." << std::endl;
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

                if(rcvinfo.rcv_assoc_id == SCTP_FUTURE_ASSOC || rcvinfo.rcv_assoc_id == SCTP_ALL_ASSOC || rcvinfo.rcv_assoc_id == SCTP_CURRENT_ASSOC) {
                    std::cerr << "sctp-server.cpp:307:rcv_assoc_id is not valid!" << std::endl;
                    throw "what?";
                }
                acquire();
                auto it = std::find_if(cbegin(), cend(), [&](auto& ptr){
                    return *(std::static_pointer_cast<sctp_transport::SctpSession>(ptr)) == stream_id;
                });
                release();
                std::shared_ptr<sctp_transport::SctpSession> sctp_session;
                if(it == cend()){
                    // Create a new session.
                    sctp_session = std::make_shared<sctp_transport::SctpSession>(*this, stream_id, socket_);
                    acquire();
                    push_back(sctp_session);
                    release();
                    // Accept and return a new context (similar to the berkeley sockets accept call.)
                } else {
                    sctp_session = std::static_pointer_cast<sctp_transport::SctpSession>(*it);
                }
                std::string received_data(buf_.data(), len);
                // std::cerr << "sctp-server.cpp:370:received_data=" << received_data << std::endl;
                sctp_session->read(ec, received_data);

                // Call the read function callback.
                fn(ec, sctp_session);
            } else {
                std::cerr << "sctp-server.cpp:362:0 length read from the sctp socket." << std::endl;
            }
            socket_.async_wait(
                transport::protocols::sctp::socket::wait_type::wait_read,
                [&, fn](const boost::system::error_code& ec){
                    read(fn, ec);
                }
            );
            return;
        } else {
            std::cerr << "sctp-server.cpp:327:async_wait for read has an error:" << ec.message() << std::endl;
            std::shared_ptr<sctp_transport::SctpSession> empty_session;
            fn(ec, empty_session);
            socket_.async_wait(
                transport::protocols::sctp::socket::wait_type::wait_read,
                [&, fn](const boost::system::error_code& ec){
                    read(fn, ec);
                }
            );
            return;
        }
    }

    SctpServer::~SctpServer(){
        stop();
        int ec = close(socket_.native_handle());
        if(ec == -1){
            perror("sctp socket");
        }
    }
}