#ifndef HTTP_SERVER_TESTS_HPP
#define HTTP_SERVER_TESTS_HPP
#include "../../../src/application-servers/http/http-server.hpp"
namespace tests
{
    class HttpServerTests
    {
    public:
        operator bool(){ return passed_; }
    private:
        bool passed_;
    };
}
#endif