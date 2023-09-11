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

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::BigNum)
      : passed_{false},
        req_{},
        chunk_{}
    {
        http::HttpBigNum num1{};
        http::HttpBigNum num2{};
        const std::size_t max = std::numeric_limits<std::size_t>::max();

        // check equality overloads.
        num1 = {25};
        num2 = {25};
        if(num1 != num2){
            return;
        }

        num1 = {25, 25};
        num2 = {25};
        if(num1 == num2){
            return;
        }

        num1 = {25,24};
        num2 = {25};
        if(num1 == num2){
            return;
        }

        // check inequality overloads.
        num1 = {24};
        num2 = {25};
        if(num1 > num2 || num1 >= num2){
            return;
        }

        num1 = {24, 25};
        num2 = {25};
        if(num1 < num2 || num1 <= num2){
            return;
        }

        num1 = {25, 24};
        num2 = {25};
        if(num1 < num2 || num1 <= num2){
            return;
        }

        //check pre and post increments.
        num1 = {1,1};
        num2 = {1,2};
        if(++num1 != num2){
            return;
        }

        num1 = {max};
        num2 = {1,0};
        if(++num1 != num2){
            return;
        }

        num1 = {1,1};
        num2 = {1,2};
        if(num1++ != num2){
            return;
        }

        num1 = {max};
        num2 = {1,0};
        if(num1++ != num2){
            return;
        }

        //check pre and post decrements.
        num1 = {1,1};
        num2 = {1,0};
        if(--num1 != num2){
            return;
        }

        num1 = {1,0};
        num2 = {max};
        if(--num1 != num2){
            return;
        }

        num1 = {1,1};
        num2 = {1,0};
        if(num1-- != num2){
            return;
        }

        num1 = {1,0};
        num2 = {max};
        if(num1-- != num2){
            return;
        }

        // Check unary integer addition.
        num1 = {1,0};
        num2 = {1,2};
        num1 += 2;
        if(num1 != num2){
            return;
        }

        num1 = {max-1};
        num2 = {1,0};
        num1 += 2;
        if(num1 != num2){
            return;
        }

        // check unary vector addition.
        num1 = {1,0};
        num2 = {2,1};
        http::HttpBigNum tmp{1,1};
        num1 += tmp;
        if(num1 != num2){
            return;
        }

        num1 = {max};
        tmp = {max,max};
        num2 = {1, 0, max-1};
        num1 += tmp;
        if(num1 != num2){
            return;
        }

        // check binary addition.
        num1 = {1,0};
        tmp = {1,1};
        num2 = {2,1};
        if( (num1 + tmp) != num2 ){
            return;
        }

        num1 = {max};
        tmp = {max,max};
        num2 = {1, 0, max-1};
        if( (num1 + tmp) != num2){
            return;
        }

        num1 = {1,0};
        num2 = {1,2};
        if( (num1 + 2) != num2){
            return;
        }

        num1 = {max-1};
        num2 = {1,0};
        if( (num1 + 2) != num2){
            return;
        }

        // check unary subtraction.
        num1 = {1,1};
        tmp = {1,2};
        num1 -= tmp;
        num2 = {0};
        if(num1 != num2){
            return;
        }

        num1 = {1,1};
        tmp = {9};
        num1 -= tmp;
        num2 = {max-8};
        if(num1 != num2){
            return;
        }

        num1 = {1,1};
        num2 = {max-8};
        num1 -= 9;
        if(num1 != num2){
            return;
        }

        // check binary subtraction.
        num1 = {1,1};
        tmp = {1,2};
        num2 = {0};
        if( (num1 - tmp) != num2){
            return;
        }

        tmp = {9};
        num2 = {max-8};
        if( (num1 - tmp) != num2){
            return;
        }

        if( (num1 - 9) != num2){
            return;
        }

        // Check hex string constructor.
        std::string hexstr("3E8");
        http::HttpBigNum num_from_hex(hexstr);
        num2 = {1000};
        if(num_from_hex != num2){
            return;
        }

        hexstr = std::string("1FFFFFFFFFFFFFFFF");
        http::HttpBigNum num_from_hex2(hexstr);
        num2 = {1, max};
        if(num_from_hex2 != num2){
            return;
        }

        passed_ = true;
    }
}