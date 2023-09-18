#ifndef HTTP_REQUESTS_TESTS_HPP
#define HTTP_REQUESTS_TESTS_HPP
#include "../../../src/application-servers/http/http-requests.hpp"
namespace tests
{
    class HttpRequestsTests
    {
    public:
        constexpr static struct ReadChunk{} test_read_chunk{};
        constexpr static struct BigNum{} test_big_num{};
        constexpr static struct WriteChunk{} test_write_chunk{};
        constexpr static struct ReadHeader{} test_read_header{};
        constexpr static struct WriteHeader{} test_write_header{};
        constexpr static struct RequestStreamExtraction{} test_request_stream_extraction{};
        constexpr static struct ResponseStreamExtraction{} test_response_stream_extraction{};
        constexpr static struct RequestStreamInsertion{} test_request_stream_insertion{};
        constexpr static struct ResponseStreamInsertion{} test_response_stream_insertion{};


        explicit HttpRequestsTests(ReadChunk);
        explicit HttpRequestsTests(BigNum);
        explicit HttpRequestsTests(WriteChunk);
        explicit HttpRequestsTests(ReadHeader);
        explicit HttpRequestsTests(WriteHeader);
        explicit HttpRequestsTests(RequestStreamExtraction);
        explicit HttpRequestsTests(ResponseStreamExtraction);
        explicit HttpRequestsTests(RequestStreamInsertion);
        explicit HttpRequestsTests(ResponseStreamInsertion);

        operator bool(){ return passed_; }
    private:
        bool passed_;
        http::HttpRequest req_;
        http::HttpChunk chunk_;
    };
}
#endif