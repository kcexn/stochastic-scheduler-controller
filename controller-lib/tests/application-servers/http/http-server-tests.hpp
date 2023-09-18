#ifndef HTTP_SERVER_TESTS_HPP
#define HTTP_SERVER_TESTS_HPP
#include "../../../src/application-servers/http/http-server.hpp"
#include "../../../src/application-servers/http/http-session.hpp"
#include "../../../src/transport-servers/unix-server/unix-server.hpp"

namespace tests
{
    class HttpServerTests
    {
    public:
        constexpr static struct ConstructServer{} test_construct_server{};
        constexpr static struct ConstructSession{} test_construct_session{};
        constexpr static struct ConstructUnixBoundSession{} test_construct_unix_bound_session{};
        constexpr static struct UnixBoundReadWrite{} test_unix_bound_read_write{};
        constexpr static struct UnixBoundClientWrite{} test_unix_client_write{};

        explicit HttpServerTests(ConstructServer);
        explicit HttpServerTests(ConstructSession);
        explicit HttpServerTests(ConstructUnixBoundSession);
        explicit HttpServerTests(UnixBoundReadWrite);
        explicit HttpServerTests(UnixBoundClientWrite);

        operator bool(){ return passed_; }
    private:
        bool passed_;
    };
}
#endif