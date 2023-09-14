#include "http-requests-tests.hpp"
#include <iostream>
#include <charconv>
#include <sstream>
#include <limits>
namespace tests
{
    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::ReadChunk)
      : passed_{false},
        req_{},
        chunk_{}
    {
        std::stringstream ss1("16\r\n{\"msg\":\"Hello World!\"}\r\n");
        std::string cmp("{\"msg\":\"Hello World!\"}");
        ss1 >> chunk_;
        if(chunk_.chunk_data != cmp){
            return;
        }

        
        http::HttpChunk last_chunk{};
        std::stringstream last_ss("0\r\n\r\n");
        cmp.clear();
        last_ss >> last_chunk;
        if(last_chunk.chunk_data != cmp){
            return;
        }


        http::HttpChunk multiple_stream{};
        std::stringstream ss(std::ios_base::in | std::ios_base::out | std::ios_base::app);
        std::string first("16\r\n{\"msg\"");
        std::string second(":\"Hello World!\"}\r\n");
        cmp = "{\"msg\":\"Hello World!\"}";
        ss << first;
        ss >> multiple_stream;
        ss << second;
        ss >> multiple_stream;
        if(cmp != multiple_stream.chunk_data){
            return;
        }
        passed_ = true;
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

        http::HttpBigNum tmp2;
        num2 = {1};
        if(++tmp2 != num2){
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
        http::HttpBigNum num_from_hex(http::HttpBigNum::hex, hexstr);
        num2 = {1000};
        if(num_from_hex != num2){
            return;
        }

        hexstr = std::string("1FFFFFFFFFFFFFFFF");
        http::HttpBigNum num_from_hex2(http::HttpBigNum::hex, hexstr);
        num2 = {1, max};
        if(num_from_hex2 != num2){
            return;
        }

        std::string decstr("300");
        http::HttpBigNum num_from_hex3(http::HttpBigNum::dec, decstr);
        num2 = {300};
        if(num_from_hex3 != num2){
            return;
        }

        decstr = "300000000000000000000";
        http::HttpBigNum num_from_hex4(http::HttpBigNum::dec, decstr);
        num2 = {16, 4852094820647174144};
        if(num_from_hex4 != num2){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::WriteChunk)
      : passed_{false},
        req_{},
        chunk_{
            {22},
            std::string("{\"msg\":\"Hello World!\"}")
        }
    {
        std::string cmp("16\r\n{\"msg\":\"Hello World!\"}\r\n");
        std::stringstream ss;
        ss << chunk_;
        if( cmp != ss.str() ){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::ReadHeader)
      : passed_{false},
        req_{},
        chunk_{}
    {

        // Simple header parsing test.
        http::HttpHeader header{};
        std::stringstream ss("Content-Type: application/json\r\n");
        http::HttpHeaderField name_cmp = http::HttpHeaderField::CONTENT_TYPE;
        std::string cmp("application/json");
        ss >> header;
        if(!(header.field_name == name_cmp) || !(header.field_value == cmp)){
            return;
        }

        // Unknown header field.
        std::stringstream ss1("  Unknown  : unknown  fields\r\n");
        header = {};
        ss1 >> header;
        if(!(header.field_name == http::HttpHeaderField::UNKNOWN) || !(header.field_value == "unknown  fields")){
            return;
        }

        // Header field name trailing spaces with known header field name.
        std::stringstream ss2("   Accept  : application/json\r\n");
        header = {};
        ss2 >> header;
        if(!(header.field_name == http::HttpHeaderField::ACCEPT) || !(header.field_value == "application/json")){
            return;
        }

        // Last header.
        std::stringstream ss3("         \r\n");
        header = {};
        ss3 >> header;
        if(!(header.field_name == http::HttpHeaderField::END_OF_HEADERS) || !(header.field_value == "")){
            return;
        }

        // Multistream header.
        std::stringstream ss4;
        std::string first("     Accept        ");
        std::string second(":    application/json\r\n");
        header = {};
        ss4 << first;
        ss4 >> header;
        ss4 << second;
        ss4 >> header;
        if(!(header.field_name == http::HttpHeaderField::ACCEPT) || !(header.field_value == "application/json")){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::WriteHeader)
      : passed_{false},
        req_{},
        chunk_{}
    {
        http::HttpHeader header{
            http::HttpHeaderField::CONTENT_TYPE,
            std::string("application/json")
        };
        std::stringstream ss;
        ss << header;
        if(!(ss.str() == "Content-Type: application/json\r\n")){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::ReadRequest)
      : passed_{false},
        req_{},
        chunk_{}
    {
        http::HttpRequest req{};
        std::stringstream ss("GET /run HTTP/1.1\r\nContent-Length: 22\r\nContent-Type: application/json\r\n\r\n{\"msg\":\"Hello World!\"}");
        ss >> req;
        if(req.verb != http::HttpVerb::GET 
                || req.route != "/run" 
                || req.headers[0].field_name != http::HttpHeaderField::CONTENT_LENGTH
                || req.headers[1].field_name != http::HttpHeaderField::CONTENT_TYPE
                || req.chunks[0].chunk_size != http::HttpBigNum{22}
                || req.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"
        ){
            return;
        }

        // Multiple stream.
        req = http::HttpRequest{};
        std::stringstream ss1;
        std::string first("POST ");
        std::string second("/run HTTP/1.1\r\n");
        std::string third("Content-Length: 22\r\nContent-Type: application/json\r\n");
        std::string fourth("\r\n");
        std::string fifth("{\"msg\":\"Hello");
        std::string sixth(" World!\"}");
        ss1 << first;
        ss1 >> req;
        ss1 << second;
        ss1 >> req;
        ss1 << third;
        ss1 >> req;
        ss1 << fourth;
        ss1 >> req;
        ss1 << fifth;
        ss1 >> req;
        ss1 << sixth;
        ss1 >> req;
        if(req.verb != http::HttpVerb::POST
                || req.route != "/run"
                || req.headers[0].field_name != http::HttpHeaderField::CONTENT_LENGTH
                || req.headers[1].field_name != http::HttpHeaderField::CONTENT_TYPE
                || req.chunks[0].chunk_size != http::HttpBigNum{22}
                || req.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"
        ){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::WriteRequest)
      : passed_{false},
        req_{},
        chunk_{}
    {
        http::HttpRequest req{
            http::HttpVerb::GET,
            "/run",
            http::HttpVersion::V1_1,
            {
                {http::HttpHeaderField::HOST, "localhost"},
                {http::HttpHeaderField::ACCEPT, "*/*"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {http::HttpBigNum{0}, ""}
            }
        };
        std::stringstream ss;
        ss << req;
        std::string cmp("GET /run HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\n");
        if(ss.str() != cmp){
            return;
        }

        ss.seekp(0);
        ss.seekg(0);

        req = {
            http::HttpVerb::DELETE,
            "/init",
            http::HttpVersion::V1,
            {
                {http::HttpHeaderField::HOST, "localhost"},
                {http::HttpHeaderField::ACCEPT, "*/*"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}           
            },
            {
                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        ss << req;
        cmp = "DELETE /init HTTP/1.0\r\nHost: localhost\r\nAccept: */*\r\n\r\n";
        if(ss.str() != cmp){
            return;
        }

        ss.seekp(0);
        ss.seekg(0);

        req = {
            http::HttpVerb::TRACE,
            "/health-check",
            http::HttpVersion::V1_1,
            {
                {http::HttpHeaderField::HOST, "localhost"},
                {http::HttpHeaderField::ACCEPT, "*/*"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}           
            },
            {
                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        ss << req;
        cmp = "TRACE /health-check HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\n";
        if(ss.str() != cmp){
            return;
        }

        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(HttpRequestsTests::WriteResponse)
      : passed_{false},
        req_{},
        chunk_{}
    {
        http::HttpResponse res{
            http::HttpVersion::V1_1,
            http::HttpStatus::OK,
            {
                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        std::stringstream ss;
        ss << res;
        std::string cmp("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 22\r\n\r\n{\"msg\":\"Hello World!\"}");
        if(ss.str() != cmp){
            return;
        }
        passed_ = true;
    }
}