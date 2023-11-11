#ifndef OWLIB_SCTP_SESSION_HPP
#define OWLIB_SCTP_SESSION_HPP
#include <system_error>
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
        void async_write(const boost::asio::const_buffer&, const std::function<void(const std::error_code& ec)>&) override;
        void close() override;

        void set(const transport::protocols::sctp::assoc_t& assoc_id ) { acquire(); id_.assoc = assoc_id; release(); return; }
        transport::protocols::sctp::assoc_t get_assoc() { acquire(); transport::protocols::sctp::assoc_t tmp = id_.assoc; release(); return tmp; }
        const transport::protocols::sctp::assoc_t& assoc() const { return id_.assoc; }
        transport::protocols::sctp::sid_t get_sid() { acquire(); auto tmp = id_.sid; release(); return tmp; }
        transport::protocols::sctp::sid_t& sid() { return id_.sid; }
        const transport::protocols::sctp::socket& socket() const { return socket_; }

        bool operator==(const SctpSession& other);
        bool operator==(const transport::protocols::sctp::stream_t& stream);
        bool operator!=(const SctpSession& other);
        bool operator!=(const transport::protocols::sctp::stream_t& stream);
    private:
        std::function<void(boost::system::error_code ec, std::size_t length)> read_fn_;
        void write_(std::shared_ptr<std::vector<char> >, const std::function<void(const std::error_code& ec)>, const boost::system::error_code& ec);

        boost::system::error_code read_ec_;
        std::size_t read_len_;
        transport::protocols::sctp::stream_t id_;
        transport::protocols::sctp::socket& socket_;
    };
}
#endif