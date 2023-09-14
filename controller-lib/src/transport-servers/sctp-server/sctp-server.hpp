#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP
#include "../server/server.hpp"
#include "sctp.hpp"

namespace sctp_transport{
    class SctpServer: public server::Server
    {
    public:
        SctpServer(boost::asio::io_context& ioc);
        SctpServer(boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);

        
        void init(std::function<void(const boost::system::error_code& ec)>);
        void stop();
        

        ~SctpServer();
    private:
        void read(std::function<void(const boost::system::error_code& ec)>, const boost::system::error_code& ec);

        constexpr static std::size_t buflen_ = 65536;
        std::array<char, buflen_> buf_;
        std::array<char, buflen_> cbuf_;
        transport::protocols::sctp::socket socket_;
    };
    
}
#endif