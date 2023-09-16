#ifndef SERVER_TESTS_HPP
#define SERVER_TESTS_HPP
#include "../../../src/transport-servers/unix-server/unix-server.hpp"

namespace tests{
    class UnixServerTest
    {
    public:
        constexpr static struct TestPushBack{} test_push_back{};
        constexpr static struct TestAcceptUnix{} test_accept_unix{};
        constexpr static struct TestContextRun{} test_context_run{};
        constexpr static struct TestReadWrite{} test_read_write{};
        constexpr static struct TestEraseSession{} test_erase_session{};
        constexpr static struct TestUnixConnect{} test_unix_connect{};

        UnixServerTest(boost::asio::io_context& ioc); // minimal constructor test.
        UnixServerTest(boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint); // Open endpoint constructor.
        explicit UnixServerTest(TestPushBack, boost::asio::io_context& ioc);
        explicit UnixServerTest(TestContextRun, boost::asio::io_context& ioc);
        explicit UnixServerTest(TestAcceptUnix, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint);
        explicit UnixServerTest(TestReadWrite, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint);
        explicit UnixServerTest(TestEraseSession, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint);
        explicit UnixServerTest(TestUnixConnect, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint);

        operator bool(){ return passed_; }
    private:
        bool passed_;
        UnixServer::unix_server server_;
    };

    class UnixSessionTest
    {
    public:
        constexpr static struct TestMoveConstruct{} test_move_construct{};

        UnixSessionTest(boost::asio::io_context& ioc, server::Server& server);
        explicit UnixSessionTest(TestMoveConstruct, boost::asio::local::stream_protocol::socket&& socket, server::Server& server);

        operator bool(){ return passed_; }
    private:
        bool passed_;
        UnixServer::unix_session session_;
    };
}
#endif