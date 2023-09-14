#ifndef SCTP_SESSION_HPP
#define SCTP_SESSION_HPP
#include <boost/asio.hpp>
#include "../server/session.hpp"

namespace sctp_transport{
    class SctpSession : public server::Session
    {
    public:
        SctpSession(){}
        void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn) override {}
        void async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn) override {}
        void close() override {}
        ~SctpSession() = default;
    private:
    }
}
#endif