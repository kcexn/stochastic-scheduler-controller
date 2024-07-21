#ifndef OWLIB_SCTP_SERVER_HPP
#define OWLIB_SCTP_SERVER_HPP
#include "../server/server.hpp"
#include "../server/session.hpp"
#include "sctp.hpp"

namespace sctp_transport{
    class SctpSession;

    struct PendingConnect {
        std::shared_ptr<SctpSession> session;
        std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)> cb;
        struct sockaddr addr;
    };

    class SctpServer: public server::Server
    {
    public:
        SctpServer(boost::asio::io_context& ioc);
        SctpServer(boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);
   
        void init(std::function<void(const boost::system::error_code&, std::shared_ptr<sctp_transport::SctpSession>)>);
        void stop();

        void async_connect(server::Remote addr, std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)> fn) override;
        void erase_pending_connect(std::shared_ptr<server::Session>); 
        ~SctpServer();
    private:
        void read(std::function<void(const boost::system::error_code&, std::shared_ptr<SctpSession>)>, const boost::system::error_code& ec);
        std::vector<PendingConnect> pending_connects_;

        std::array<char, SERVER_SESSION_MAX_BUFLEN> buf_;
        std::array<char, SERVER_SESSION_MAX_BUFLEN> cbuf_;
        transport::protocols::sctp::socket socket_;
        transport::protocols::sctp::sid_t next_stream_num_;
    };
}
#endif