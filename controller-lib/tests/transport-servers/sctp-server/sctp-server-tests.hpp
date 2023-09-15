#ifndef SCTP_SERVER_TESTS_HPP
#define SCTP_SERVER_TESTS_HPP
#include "../../../src/transport-servers/sctp-server/sctp-server.hpp"



namespace tests{
    class SctpServerTests{
    public:
        constexpr static struct DefaultConstructor{} test_constructor{};
        constexpr static struct TestSocketRead{} test_socket_read{};
        constexpr static struct TestSessionConstructor{} test_session_constructor{};
        constexpr static struct TestSessionReadWrite{} test_session_read_write{};

        explicit SctpServerTests(DefaultConstructor, boost::asio::io_context& ioc);
        explicit SctpServerTests(TestSocketRead, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);
        explicit SctpServerTests(TestSessionConstructor, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);
        explicit SctpServerTests(TestSessionReadWrite, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint);
        
        operator bool(){ return passed_; }
    private:
        bool passed_;
    };
}
#endif