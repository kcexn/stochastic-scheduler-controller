#include "http-requests-tests.hpp"
#include <iostream>
#include <charconv>
namespace tests
{
    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::MaxChunkSize)
      : passed_{false},
        req_{},
        chunk_{}
    {
        std::string hex_str(2*sizeof(std::size_t), '\0');
        std::string str_cmp(2*sizeof(std::size_t), 'f');
        std::to_chars_result res = std::to_chars(hex_str.data(), hex_str.data()+hex_str.size(), http::HttpChunk::max_chunk_size, 16);
        if( hex_str == str_cmp ){
            passed_ = true;
        }
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::ReadChunk)
      : passed_{false},
        req_{},
        chunk_{}
    {
        std::stringstream ss("16\r\n{\"msg\":\"Hello World!\"}\r\n");
        ss >> chunk_;
        std::cout << std::hex << chunk_.chunk_size << std::endl;
        std::cout << chunk_.chunk_data << std::endl;

        http::HttpChunk last_chunk{};
        std::stringstream last_ss("0\r\n\r\n");
        last_ss >> last_chunk;
        std::cout << std::hex << last_chunk.chunk_size << std::endl;
        std::cout << last_chunk.chunk_data << std::endl;


        http::HttpChunk multiple_stream{};
        std::stringstream first_ss("16\r\n{\"msg\"", std::ios_base::in | std::ios_base::out | std::ios_base::app);
        std::string second_ss(":\"Hello World!\"}\r\n");
        first_ss >> multiple_stream;
        first_ss << second_ss;
        first_ss >> multiple_stream;

        std::cout << std::hex << multiple_stream.chunk_size << std::endl;
        std::cout << multiple_stream.chunk_data << std::endl;

        if (multiple_stream.chunk_complete && last_chunk.chunk_complete && chunk_.chunk_complete){
            passed_ = true;
        }
    }
}