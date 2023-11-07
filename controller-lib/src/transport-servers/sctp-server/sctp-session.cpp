#include "sctp-session.hpp"
#include <iostream>

namespace sctp_transport{
    void SctpSession::async_read(std::function<void(boost::system::error_code ec, std::size_t length)>){
        return;
        // read_fn_ = std::move(fn);
    }

    void SctpSession::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn) {
        auto self = shared_from_this();
        std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
        std::memcpy(write_data_ptr->data(), write_buffer.data(), write_buffer.size());
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_write,
            boost::asio::bind_cancellation_slot(
                stop_signal_.slot(),
                [&, write_data_ptr, fn, self](const boost::system::error_code& ec){
                    write_(write_data_ptr, fn, ec);
                }
            )
        );
    }

    void SctpSession::write_(std::shared_ptr<std::vector<char> > write_data, const std::function<void()> fn, const boost::system::error_code& ec){
        if(!ec){
            using namespace transport::protocols;
            sctp::iov iobuf= {
                write_data->data(),
                write_data->size()
            };
            sctp::sndinfo sndinfo = {
                id_.sid,
                0,
                0,
                0,
                id_.assoc
            };
            transport::protocols::sctp::status status = {};
            status.sstat_assoc_id = id_.assoc;
            socklen_t optsize = sizeof(status);
            int ec = getsockopt(socket_.native_handle(), IPPROTO_SCTP, SCTP_STATUS, &status, &optsize);
            if(ec == -1){
                switch(errno)
                {
                    case EINVAL:
                        return;
                    default:
                        std::cerr << "sctp-session.cpp:51:getsockopt failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "what?";
                }
            }
            switch(status.sstat_state)
            {
                case SCTP_CLOSED:
                    // std::cerr << "sctp-session.cpp:48:SCTP_CLOSED STATE" << std::endl;
                    return;
                case SCTP_EMPTY:
                    // std::cerr << "sctp-session.cpp:51:SCTP_EMPTY STATE" << std::endl;
                    return;
                case SCTP_COOKIE_WAIT:
                {
                    // std::cerr << "sctp-session.cpp:54:SCTP_COOKIE_WAIT" << std::endl;
                    boost::asio::const_buffer buf(write_data->data(), write_data->size());
                    return async_write(buf, fn);
                }
                case SCTP_COOKIE_ECHOED:
                {
                    // std::cerr << "sctp-session.cpp:64:SCTP_COOKIE_ECHOED" << std::endl;
                    boost::asio::const_buffer buf(write_data->data(), write_data->size());
                    return async_write(buf, fn);               
                }
                case SCTP_ESTABLISHED:
                    break;
                case SCTP_SHUTDOWN_PENDING:
                    // std::cerr << "sctp-session.cpp:77:SCTP_SHUTDOWN_PENDING" << std::endl;
                    return;
                case SCTP_SHUTDOWN_SENT:
                    // std::cerr << "sctp-session.cpp:80:SCTP_SHUTDOWN_SENT" << std::endl;
                    return;
                case SCTP_SHUTDOWN_RECEIVED:
                {   
                    // std::cerr << "sctp-session.cpp:83:SCTP_SHUTDOWN_RECEIVED" << std::endl;
                    return;
                }
                case SCTP_SHUTDOWN_ACK_SENT:
                    // std::cerr << "sctp-session.cpp:86:SCTP_SHUTDOWN_ACK_SENT" << std::endl;
                    return;
                default:
                    std::cerr << "sctp-session.cpp:115:unrecognized status.sstat_state value:" << status.sstat_state << std::endl;
                    throw "what?";
            }
            // The union guarantees that the cmsghdr will have enough
            // space for byte alignment requirements.
            union {
                std::array<char, CMSG_SPACE(sizeof(sctp::sndinfo))> cbuf_;
                sctp::cmsghdr align;
            } u;

            sctp::msghdr msg = {
                nullptr,
                0,
                &iobuf,
                1,
                u.cbuf_.data(),
                u.cbuf_.size(),
                0        
            };
            sctp::cmsghdr* cmsg;
            cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = IPPROTO_SCTP;
            cmsg->cmsg_type = SCTP_SNDINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(sndinfo));
            std::memcpy(CMSG_DATA(cmsg), &sndinfo, sizeof(sndinfo));
            int len = sendmsg(socket_.native_handle(), &msg, MSG_NOSIGNAL);
            if(len == -1){
                switch(errno)
                {
                    case EWOULDBLOCK:
                    {
                        boost::asio::const_buffer buf(write_data->data(), write_data->size());
                        return async_write(buf, fn);
                    }
                    case EINTR:
                    {
                        boost::asio::const_buffer buf(write_data->data(), write_data->size());
                        return async_write(buf, fn);
                    }
                    case EINVAL:
                        return;
                    default:
                    {
                        struct timespec ts = {};
                        int errsv = errno;
                        int status = clock_gettime(CLOCK_REALTIME, &ts);
                        if(status == -1){
                            std::cerr << "sctp-session.cpp:183:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            std::cerr << "sctp-session.cpp:184:sendmsg failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                            return;
                        }
                        std::cerr << "sctp-session.cpp:187:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":sendmsg failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                        return;
                    }
                }
            } else {
                std::size_t remaining_bytes = write_data->size() - len;
                if(remaining_bytes > 0){
                    boost::asio::const_buffer buf(write_data->data() + len, remaining_bytes);
                    return async_write(buf, fn);
                } else {
                    fn();
                }
            }
        } else {
            std::cerr << "sctp-session.cpp:214:sctp_session async wait_write error:" << ec.message() << std::endl;
        }
    }

    void SctpSession::read(const boost::system::error_code&, const std::string& received_data){
        acquire_stream() << received_data;
        release_stream();
        // if (read_fn_){
        //     read_fn_(ec, received_data.size());
        // }
        return;
    }

    void SctpSession::close(){
        erase();
    }

    bool SctpSession::operator==(const SctpSession& other){
        return id_.assoc == other.id_.assoc && id_.sid == other.id_.sid;
    }
    bool SctpSession::operator==(const transport::protocols::sctp::stream_t& stream){
        return id_.assoc == stream.assoc && id_.sid == stream.sid;
    }
    bool SctpSession::operator!=(const SctpSession& other){
        return id_.assoc != other.id_.assoc && id_.sid != other.id_.sid;
    }
    bool SctpSession::operator!=(const transport::protocols::sctp::stream_t& stream){
        return id_.assoc != stream.assoc && id_.sid != stream.sid;
    }

}