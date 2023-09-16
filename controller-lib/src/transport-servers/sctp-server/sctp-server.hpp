#ifndef OWLIB_SCTP_SERVER_HPP
#define OWLIB_SCTP_SERVER_HPP
#include "../server/server.hpp"
#include "../server/session.hpp"
#include "sctp.hpp"

namespace sctp_transport{
    class SctpSession;

    class SctpServer: public server::Server
    {
    public:
        SctpServer(boost::asio::io_context& ioc);
        SctpServer(boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);
   
        void init(std::function<void(const boost::system::error_code&, std::shared_ptr<sctp_transport::SctpSession>)>);
        void stop();

        std::shared_ptr<server::Session> async_connect(server::Remote addr, std::function<void(const boost::system::error_code&)> fn);
        
        ~SctpServer();
    private:
        void read(std::function<void(const boost::system::error_code&, std::shared_ptr<sctp_transport::SctpSession>)>, const boost::system::error_code& ec);
        std::vector<transport::protocols::sctp::assoc_t> assoc_table_;

        std::array<char, server::Session::max_buflen> buf_;
        std::array<char, server::Session::max_buflen> cbuf_;
        transport::protocols::sctp::socket socket_;
    };
}
#endif