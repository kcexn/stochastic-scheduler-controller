#ifndef OWLIB_SCTP_SESSION_HPP
#define OWLIB_SCTP_SESSION_HPP
#include <boost/asio.hpp>
#include <boost/context/fiber.hpp>
#include "../server/session.hpp"
#include "sctp-server.hpp"
#include "sctp.hpp"

namespace sctp_transport{
    class SctpSession : public server::Session
    {
    public:
        SctpSession(SctpServer& server, transport::protocols::sctp::stream_t id, transport::protocols::sctp::socket& socket): server::Session(server), id_{id}, socket_(socket) {}

        void read(const boost::system::error_code& ec, const std::string& received_data);
        void async_read(std::function<void(boost::system::error_code ec, std::size_t length)>) override;
        void async_write(const boost::asio::const_buffer&, const std::function<void()>&) override;
        void close() override;
        ~SctpSession() = default; 

        bool operator==(const SctpSession& other);
        bool operator==(const transport::protocols::sctp::stream_t& stream);
        bool operator!=(const SctpSession& other);
        bool operator!=(const transport::protocols::sctp::stream_t& stream);
    private:
        std::function<void(boost::system::error_code ec, std::size_t length)> read_fn_;
        void write_(std::shared_ptr<std::vector<char> >, const std::function<void()>, const boost::system::error_code& ec);

        boost::system::error_code read_ec_;
        std::size_t read_len_;
        transport::protocols::sctp::stream_t id_;
        transport::protocols::sctp::socket& socket_;
    };
}
#endif