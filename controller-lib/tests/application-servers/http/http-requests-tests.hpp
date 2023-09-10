#ifndef HTTP_REQUESTS_TESTS_HPP
#define HTTP_REQUESTS_TESTS_HPP
#include "../../../src/application-servers/http/http-requests.hpp"
namespace tests
{
    class HttpRequestsTests
    {
    public:
        constexpr static struct MaxChunkSize{} test_max_chunk_size{};
        constexpr static struct ReadChunk{} test_read_chunk{};
        explicit HttpRequestsTests(MaxChunkSize);
        explicit HttpRequestsTests(ReadChunk);

        operator bool(){ return passed_; }
    private:
        bool passed_;
        http::HttpRequest req_;
        http::HttpChunk chunk_;
    };
}
#endif