#include "sctp-server.hpp"
#include "sctp-session.hpp"
#include <cerrno>
#include <iostream>
#include <cstdint>

namespace sctp_transport{
    static const std::uint16_t MAX_SCTP_STREAMS = UINT16_MAX;

    SctpServer::SctpServer(boost::asio::io_context& ioc)
      : server::Server(ioc),
        socket_(ioc),
        next_stream_num_(0)
    {
        buf_.fill(0);
        cbuf_.fill(0);
    }

    SctpServer::SctpServer(boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
      : server::Server(ioc),
        socket_(ioc),
        next_stream_num_(0)
    {
        buf_.fill(0);
        cbuf_.fill(0);
        int sockfd = socket(AF_INET, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_SCTP);
        if(sockfd == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:28:sctp_server failed to open:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }

        static constexpr int so_reuseaddr = 1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:38:SO_REUSEADDR socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        static constexpr int recvrcvinfo = 1;
        if(setsockopt(sockfd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &recvrcvinfo, sizeof(recvrcvinfo)) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:47:SCTP_RECVRCVINFO socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        static constexpr struct sctp_event subscribe = {
            SCTP_ALL_ASSOC,
            SCTP_ASSOC_CHANGE,
            1
        };
        if(setsockopt(sockfd, IPPROTO_SCTP, SCTP_EVENT, &subscribe, sizeof(subscribe)) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:60:SCTP_EVENT socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        static constexpr transport::protocols::sctp::sockopt_initmsg initmsg = {
            MAX_SCTP_STREAMS,
            MAX_SCTP_STREAMS, 
            0,
            3000
        };
        if(setsockopt(sockfd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:74:SCTP_INITMSG socket option failed to set:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        struct sockaddr_in local_address = {};
        local_address.sin_family = AF_INET;
        local_address.sin_port = htons(endpoint.port());
        local_address.sin_addr.s_addr = htonl(endpoint.address().to_v4().to_uint());
        if(bind(sockfd, (const struct sockaddr*)(&local_address), sizeof(local_address)) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:69:bind failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
        if(listen(sockfd, 4096) == -1){
            switch(errno)
            {
                default:
                    std::cerr << "sctp-server.cpp:77:listen failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
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
                std::vector<transport::protocols::sctp::sid_t> used_stream_nums(MAX_SCTP_STREAMS);
                transport::protocols::sctp::sid_t s_offset = 0;
                acquire();
                // Append all of the streams in pending connects.
                for(auto& pc: pending_connects_){
                    auto addr_in = (struct sockaddr_in*)(&pc.addr);
                    if(rmt.ipv4_addr.address.sin_addr.s_addr == addr_in->sin_addr.s_addr && rmt.ipv4_addr.address.sin_port == addr_in->sin_port){
                        used_stream_nums.push_back(pc.session->get_sid());
                    }
                }
                // Find an unused stream number.
                for(s_offset = 0; s_offset < MAX_SCTP_STREAMS; ++s_offset){
                    auto it = std::find(used_stream_nums.cbegin(), used_stream_nums.cend(), next_stream_num_);
                    if(it == used_stream_nums.cend()){
                        break;
                    }
                    next_stream_num_ = (next_stream_num_ + 1)%MAX_SCTP_STREAMS;
                }
                transport::protocols::sctp::stream_t stream = {
                    SCTP_FUTURE_ASSOC,
                    next_stream_num_
                };
                if(s_offset == MAX_SCTP_STREAMS){
                    std::cerr << "sctp-server.cpp:139:SCTP_OUT_OF_STREAMS:" << next_stream_num_ << std::endl;
                    release();
                    fn(boost::system::error_code(EADDRNOTAVAIL, boost::system::system_category()), session);
                } else {
                    session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
                    PendingConnect connection = {session, fn, {}};
                    std::memcpy(&connection.addr, (const struct sockaddr*)(&rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                    pending_connects_.push_back(connection);
                    release();
                    if(s_offset == 0){
                        socket_.async_wait(
                            transport::protocols::sctp::socket::wait_type::wait_write,
                            [&, session, rmt, fn](const boost::system::error_code& ec) {
                                if(!ec){
                                    int err = connect(socket_.native_handle(), (const struct sockaddr*)(&rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                                    boost::system::error_code error;
                                    if(err < 0){
                                        switch(errno)
                                        {
                                            case EINPROGRESS:
                                                break;
                                            case EISCONN:
                                            {
                                                transport::protocols::sctp::paddrinfo paddrinfo = {};
                                                socklen_t paddrinfo_size = sizeof(paddrinfo);
                                                std::memcpy(&(paddrinfo.spinfo_address), &(rmt.ipv4_addr.address), sizeof(rmt.ipv4_addr.address));
                                                err = getsockopt(socket_.native_handle(), IPPROTO_SCTP, SCTP_GET_PEER_ADDR_INFO, &paddrinfo, &paddrinfo_size);
                                                if(err == -1){
                                                    switch(errno)
                                                    {
                                                        default:
                                                            std::cerr << "sctp-server.cpp:148:getsockopt() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                            throw "what?";
                                                    }
                                                }
                                                session->set(paddrinfo.spinfo_assoc_id);
                                                acquire();
                                                push_back(session);
                                                release();
                                                fn(error, session);
                                                erase_pending_connect(session);
                                                return;
                                            }
                                            case EALREADY:
                                                break;
                                            default:
                                                std::cerr << "sctp-server.cpp:163:connect() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                error = boost::system::error_code(errno, boost::system::system_category());
                                                fn(error, session);
                                                erase_pending_connect(session);
                                                return;

                                        }
                                    }
                                } else {
                                    std::cerr << "sctp-server.cpp:172:async_wait write failed:" << ec.message() << std::endl;
                                    fn(ec, session);
                                    erase_pending_connect(session);
                                }
                                return;
                            }
                        );
                    }
                }
            } else {
                std::cerr << "sctp-server.cpp:181:getsockopt() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            }
        } else {
            /* Remote address is in the peer address table. */
            transport::protocols::sctp::assoc_t assoc_id = paddrinfo.spinfo_assoc_id;
            /* Look for the next available stream*/
            std::vector<transport::protocols::sctp::sid_t> used_stream_nums(MAX_SCTP_STREAMS);
            acquire();
            // Append all of the streams in the server.
            for(auto& session_ptr: *this){
                if(std::static_pointer_cast<SctpSession>(session_ptr)->get_assoc() == assoc_id){
                    used_stream_nums.push_back(std::static_pointer_cast<SctpSession>(session_ptr)->get_sid());
                }
            }
            // Also append all of the streams in pending connects.
            for(auto& pc: pending_connects_){
                auto addr_in = (struct sockaddr_in*)(&pc.addr);
                if(rmt.ipv4_addr.address.sin_addr.s_addr == addr_in->sin_addr.s_addr && rmt.ipv4_addr.address.sin_port == addr_in->sin_port){
                    used_stream_nums.push_back(pc.session->get_sid());
                }
            }
            transport::protocols::sctp::sid_t s_offset = 0;
            for(s_offset = 0; s_offset < MAX_SCTP_STREAMS; ++s_offset){
                auto it = std::find(used_stream_nums.cbegin(), used_stream_nums.cend(), next_stream_num_);
                if(it == used_stream_nums.cend()){
                    break;
                }
                next_stream_num_ = (next_stream_num_ + 1)%MAX_SCTP_STREAMS;
            }
            boost::system::error_code err;
            if(s_offset == MAX_SCTP_STREAMS){
                std::cerr << "sctp-server.cpp:234:SCTP_OUT_OF_STREAMS:" << next_stream_num_ << std::endl;
                err.assign(EADDRNOTAVAIL, boost::system::system_category());
            } else {
                transport::protocols::sctp::stream_t stream = {
                    assoc_id,
                    next_stream_num_
                };
                session = std::make_shared<sctp_transport::SctpSession>(*this, stream, socket_);
                push_back(session);
            }
            release();
            fn(err, session);
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
            len = recvmsg(socket_.native_handle(), &msg, 0);
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
                        std::cerr << "sctp-server.cpp:272:recvmsg failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        socket_.async_wait(
                            transport::protocols::sctp::socket::wait_type::wait_read,
                            [&, fn](const boost::system::error_code& ec){
                                read(fn, ec);
                            }
                        );
                        return;
                    default:
                        std::cerr << "sctp-server.cpp:281:recvmsg failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
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
                                        return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                    });
                                    while(it != pending_connects_.end()){
                                        /* This is a pending connection */
                                        const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                        sctp_session->set(association);
                                        push_back(sctp_session);
                                        it->cb(error, sctp_session);
                                        pending_connects_.erase(it);
                                        it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                            sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                            return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                        });
                                    }
                                } catch (std::bad_weak_ptr& e){
                                    std::cerr << "sctp-server.cpp:321:std::bad_weak_ptr thrown:" << e.what() << std::endl;
                                    throw e;
                                }
                                release();
                                /* Otherwise it's a brand new incoming connection. */
                                break;
                            }
                            case SCTP_COMM_LOST:
                            {
                                acquire();
                                // Remove all pending connects from the pending connects table.
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                });
                                while(it != pending_connects_.end()){
                                    /* This is a pending connection */
                                    pending_connects_.erase(it);
                                    it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                        sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                        return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                    });
                                }

                                // Remove all sessions with this association from the sessions table.
                                auto session_it = std::find_if(begin(), end(), [&](auto& sp){
                                    auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                    return assoc == association;
                                });
                                while(session_it != end()){
                                    auto& sp = *session_it;
                                    sp->cancel();
                                    erase(session_it);
                                    session_it = std::find_if(begin(), end(), [&](auto& sp){
                                        auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                        return assoc == association;
                                    });
                                }
                                release();
                                // std::cerr << "sctp-server.cpp:376:SCTP COMM LOST:" << std::endl;
                                break;
                            }
                            case SCTP_RESTART:
                            {
                                acquire();
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                });
                                while(it != pending_connects_.end()){
                                    /* This is a pending connection */
                                    pending_connects_.erase(it);
                                    it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                        sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                        return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                    });
                                }

                                // Remove all sessions with this association from the sessions table.
                                auto session_it = std::find_if(begin(), end(), [&](auto& sp){
                                    auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                    return assoc == association;
                                });
                                while(session_it != end()){
                                    auto& sp = *session_it;
                                    sp->cancel();
                                    erase(session_it);
                                    session_it = std::find_if(begin(), end(), [&](auto& sp){
                                        auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                        return assoc == association;
                                    });
                                }
                                release();
                                // std::cerr << "sctp-server.cpp:333:SCTP_RESTART EVENT" << std::endl;
                                break;
                            }
                            case SCTP_SHUTDOWN_COMP:
                            {
                                // struct timespec ts = {};
                                // clock_gettime(CLOCK_REALTIME, &ts);
                                // std::cerr << "sctp-server.cpp:286:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":SCTP_SHUTDOWN_COMP EVENT" << std::endl;
                                boost::system::error_code error;
                                acquire();
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                });
                                while(it != pending_connects_.end()){
                                    /* This is a pending connection */
                                    pending_connects_.erase(it);
                                    it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                        sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                        return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                    });
                                }

                                // Remove all sessions with this association from the sessions table.
                                auto session_it = std::find_if(begin(), end(), [&](auto& sp){
                                    auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                    return assoc == association;
                                });
                                while(session_it != end()){
                                    auto& sp = *session_it;
                                    sp->cancel();
                                    erase(session_it);
                                    session_it = std::find_if(begin(), end(), [&](auto& sp){
                                        auto assoc = std::static_pointer_cast<SctpSession>(sp)->get_assoc();
                                        return assoc == association;
                                    });
                                }
                                release();
                                // std::cerr << "sctp-server.cpp:448:SCTP SHUTDOWN COMP:" << std::endl;
                                break;
                            }
                            case SCTP_CANT_STR_ASSOC:
                            {
                                // std::cerr << "sctp-server.cpp:355:SCTP_CANT_STR_ASSOC EVENT" << std::endl;
                                /* Search for the association in the pending connects table */
                                acquire();
                                auto it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                    sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                    return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                });
                                while(it != pending_connects_.cend()){
                                    /* This is a pending connection */
                                    boost::system::error_code error(ECONNREFUSED, boost::system::system_category());
                                    const std::shared_ptr<SctpSession>& sctp_session = std::static_pointer_cast<SctpSession>(it->session);
                                    it->cb(error, sctp_session);
                                    pending_connects_.erase(it);
                                    it = std::find_if(pending_connects_.begin(), pending_connects_.end(), [&](auto& pending_connection){
                                        sctp::sockaddr_in* paddr = (sctp::sockaddr_in*)(&pending_connection.addr);
                                        return (paddr->sin_addr.s_addr == addr.sin_addr.s_addr && paddr->sin_port == addr.sin_port);
                                    });
                                }
                                release();
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        std::cerr << "sctp-server.cpp:462:SCTP UNRECOGNIZED EVENT:" << snp->sn_header.sn_type << std::endl;
                        break;
                }
            } else if (len > 0) {
                sctp::cmsghdr* cmsg;
                for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)){
                    if(cmsg->cmsg_len == 0){
                        std::cerr << "sctp-server.cpp:469:cmsg_len == 0." << std::endl;
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
                    std::cerr << "sctp-server.cpp:483:rcv_assoc_id is not valid!" << std::endl;
                    throw "what?";
                }
                acquire();
                auto it = std::find_if(begin(), end(), [&](auto& ptr){
                    return *(std::static_pointer_cast<sctp_transport::SctpSession>(ptr)) == stream_id;
                });
                std::shared_ptr<sctp_transport::SctpSession> sctp_session;
                if(it == end()){
                    // Create a new session.
                    sctp_session = std::make_shared<sctp_transport::SctpSession>(*this, stream_id, socket_);
                    push_back(sctp_session);
                    // Accept and return a new context (similar to the berkeley sockets accept call.)
                } else {
                    sctp_session = std::static_pointer_cast<sctp_transport::SctpSession>(*it);
                }
                std::string received_data(buf_.data(), len);
                // std::cerr << "sctp-server.cpp:370:received_data=" << received_data << std::endl;
                sctp_session->read(ec, received_data);
                release();
                // Call the read function callback.
                fn(ec, sctp_session);
            } else {
                std::cerr << "sctp-server.cpp:509:0 length read from the sctp socket." << std::endl;
            }
            socket_.async_wait(
                transport::protocols::sctp::socket::wait_type::wait_read,
                [&, fn](const boost::system::error_code& ec){
                    read(fn, ec);
                }
            );
            return;
        } else {
            std::cerr << "sctp-server.cpp:519:async_wait for read has an error:" << ec.message() << std::endl;
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
            std::cerr << "sctp-server.cpp:536:closing the sctp socket failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        }
    }
}