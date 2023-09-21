#include "sctp-session.hpp"

namespace sctp_transport{
    void SctpSession::async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn){
        read_fn_ = std::move(fn);
    }

    void SctpSession::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn) {
        std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
        std::memcpy(write_data_ptr->data(), write_buffer.data(), write_buffer.size());
        socket_.async_wait(
            transport::protocols::sctp::socket::wait_type::wait_write,
            [&, this, write_data_ptr, fn](const boost::system::error_code& ec){
                this->write_(write_data_ptr, fn, ec);
            }
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
                perror("sendmsg failed");
            } else {
                fn();
            }
        }
    }

    void SctpSession::read(const boost::system::error_code& ec, const std::string& received_data){
        acquire_stream() << received_data;
        release_stream();
        if (read_fn_){
            read_fn_(ec, received_data.size());
        }
        return;
    }

    void SctpSession::close(){
        erase();
    }

    // SctpSession::~SctpSession()

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