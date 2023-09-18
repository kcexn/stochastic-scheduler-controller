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

    HttpRequestsTests::HttpRequestsTests(RequestStreamExtraction)
      : passed_{false}
    {
        /* Stream in Content-Length mode. */
        std::string buf1("POS");
        std::string buf2("T /run ");
        std::string buf3("HTTP");
        std::string buf4("/1.1\r\n");
        std::string buf5("Content-Type: application/json\r\n");
        std::string buf6("Content-Length: 22\r\n");
        std::string buf7("Accept: */*\r\n");
        std::string buf8("\r\n{\"msg\":\"Hello World!\"}");
        std::stringstream ss;
        http::HttpRequest req{};
        ss << buf1;
        ss >> req;
        ss << buf2;
        ss >> req;
        ss << buf3;
        ss >> req;
        ss << buf4;
        ss >> req;
        ss << buf5;
        ss >> req;
        ss << buf6;
        ss >> req;
        ss << buf7;
        ss >> req;
        ss << buf8;
        ss >> req;

        if(req.verb != http::HttpVerb::POST){
            return;
        } else if (req.route != "/run"){
            return;
        } else if (req.version != http::HttpVersion::V1_1){
            return;
        } else if (req.headers[0].field_name != http::HttpHeaderField::CONTENT_TYPE){
            return;
        } else if (req.headers[0].field_value != "application/json") {
            return;
        } else if (req.headers[1].field_name != http::HttpHeaderField::CONTENT_LENGTH){
            return;
        } else if (req.headers[1].field_value != "22"){
            return;
        } else if (req.headers[2].field_name != http::HttpHeaderField::ACCEPT){
            return;
        } else if (req.headers[2].field_value != "*/*"){
            return;
        } else if (req.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"){
            return;
        }

        /* Stream in Chunked mode. */
        std::string cbuf1("POS");
        std::string cbuf2("T /run ");
        std::string cbuf3("HTTP");
        std::string cbuf4("/1.1\r\n");
        std::string cbuf5("Content-Type: application/json\r\n");
        std::string cbuf6("Transfer-Encoding: chunked\r\n");
        std::string cbuf7("Accept: */*\r\n");
        std::string cbuf8("\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n");
        std::string cbuf9("16\r\n{\"msg1\":\"Hello World\"}\r\n");
        std::string cbuf10("0\r\n\r\n");
        std::stringstream ss1;
        http::HttpRequest req1{};
        ss1 << cbuf1;
        ss1 >> req1;
        ss1 << cbuf2;
        ss1 >> req1;
        ss1 << cbuf3;
        ss1 >> req1;
        ss1 << cbuf4;
        ss1 >> req1;
        ss1 << cbuf5;
        ss1 >> req1;
        ss1 << cbuf6;
        ss1 >> req1;
        ss1 << cbuf7;
        ss1 >> req1;
        ss1 << cbuf8;
        ss1 >> req1;
        ss1 << cbuf9;
        ss1 >> req1;
        ss1 << cbuf10;
        ss1 >> req1;

        if(req1.verb != http::HttpVerb::POST){
            return;
        } else if (req1.route != "/run"){
            return;
        } else if (req1.version != http::HttpVersion::V1_1){
            return;
        } else if (req1.headers[0].field_name != http::HttpHeaderField::CONTENT_TYPE){
            return;
        } else if (req1.headers[0].field_value != "application/json") {
            return;
        } else if (req1.headers[1].field_name != http::HttpHeaderField::TRANSFER_ENCODING){
            return;
        } else if (req1.headers[1].field_value != "chunked"){
            return;
        } else if (req1.headers[2].field_name != http::HttpHeaderField::ACCEPT){
            return;
        } else if (req1.headers[2].field_value != "*/*"){
            return;
        } else if (req1.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"){
            return;
        } else if (req1.chunks[1].chunk_data != "{\"msg1\":\"Hello World\"}"){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(ResponseStreamExtraction)
      : passed_{false}
    {
        /* Stream in Content-Length mode. */
        std::string buf1("HTTP");
        std::string buf2("/1.1 200 ");
        std::string buf3("OK\r\n");
        std::string buf4("Content-Type: application/json\r\n");
        std::string buf5("Content-Length: 22\r\n");
        std::string buf6("Connection: close\r\n");
        std::string buf7("\r\n{\"msg\":\"Hello World!\"}");
        std::stringstream ss;
        http::HttpResponse res{};
        ss << buf1;
        ss >> res;
        ss << buf2;
        ss >> res;
        ss << buf3;
        ss >> res;
        ss << buf4;
        ss >> res;
        ss << buf5;
        ss >> res;
        ss << buf6;
        ss >> res;
        ss << buf7;
        ss >> res;

        if (res.version != http::HttpVersion::V1_1){
            return;
        } else if (res.status != http::HttpStatus::OK){
            return;
        } else if (res.headers[0].field_name != http::HttpHeaderField::CONTENT_TYPE){
            return;
        } else if (res.headers[0].field_value != "application/json") {
            return;
        } else if (res.headers[1].field_name != http::HttpHeaderField::CONTENT_LENGTH){
            return;
        } else if (res.headers[1].field_value != "22"){
            return;
        } else if (res.headers[2].field_name != http::HttpHeaderField::CONNECTION){
            return;
        } else if (res.headers[2].field_value != "close"){
            return;
        } else if (res.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"){
            return;
        }

        /* Stream in Chunked mode. */
        std::string cbuf1("HTTP");
        std::string cbuf2("/1.1 200 ");
        std::string cbuf3("OK\r\n");
        std::string cbuf4("Content-Type: application/json\r\n");
        std::string cbuf5("Transfer-Encoding: chunked\r\n");
        std::string cbuf6("Connection: close\r\n");
        std::string cbuf7("\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n");
        std::string cbuf8("16\r\n{\"msg1\":\"Hello World\"}\r\n");
        std::string cbuf9("0\r\n\r\n");
        std::stringstream ss1;
        http::HttpResponse res1{};
        ss1 << cbuf1;
        ss1 >> res1;
        ss1 << cbuf2;
        ss1 >> res1;
        ss1 << cbuf3;
        ss1 >> res1;
        ss1 << cbuf4;
        ss1 >> res1;
        ss1 << cbuf5;
        ss1 >> res1;
        ss1 << cbuf6;
        ss1 >> res1;
        ss1 << cbuf7;
        ss1 >> res1;
        ss1 << cbuf8;
        ss1 >> res1;
        ss1 << cbuf9;
        ss1 >> res1;

        if (res.version != http::HttpVersion::V1_1){
            return;
        } else if (res.status != http::HttpStatus::OK){
            return;
        } else if (res1.headers[0].field_name != http::HttpHeaderField::CONTENT_TYPE){
            return;
        } else if (res1.headers[0].field_value != "application/json") {
            return;
        } else if (res1.headers[1].field_name != http::HttpHeaderField::TRANSFER_ENCODING){
            return;
        } else if (res1.headers[1].field_value != "chunked"){
            return;
        } else if (res1.headers[2].field_name != http::HttpHeaderField::CONNECTION){
            return;
        } else if (res1.headers[2].field_value != "close"){
            return;
        } else if (res1.chunks[0].chunk_data != "{\"msg\":\"Hello World!\"}"){
            return;
        } else if (res1.chunks[1].chunk_data != "{\"msg1\":\"Hello World\"}"){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(RequestStreamInsertion)
      : passed_{false}
    {
        /* Content Length Insertion. */
        http::HttpRequest req{
            http::HttpVerb::POST,
            "/run",
            http::HttpVersion::V1_1,
            {
                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                {http::HttpHeaderField::ACCEPT, "*/*"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {{}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        std::stringstream ss;
        ss << req;
        req.http_request_line_complete = true;
        req.next_header = req.headers.size();
        req.next_chunk = req.chunks.size();

        std::string cmp("POST /run HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 22\r\nAccept: */*\r\n\r\n{\"msg\":\"Hello World!\"}");
        if(ss.str() != cmp){
            return;
        }

        /* Chunked Insertion. */
        http::HttpRequest req1{
            http::HttpVerb::POST,
            "/run",
            http::HttpVersion::V1_1,
            {
                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                {http::HttpHeaderField::TRANSFER_ENCODING, "chunked"},
                {http::HttpHeaderField::ACCEPT, "*/*"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {{22}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        std::stringstream ss1;
        ss1 << req1;
        req1.http_request_line_complete = true;
        req1.next_chunk = req1.chunks.size();
        req1.next_header = req1.headers.size();
        
        std::string cmp1("POST /run HTTP/1.1\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nAccept: */*\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n");
        if(ss1.str() != cmp1){
            return;
        }
        req1.chunks.push_back(
            http::HttpChunk{{22}, "{\"msg1\":\"Hello World\"}"}
        );
        std::stringstream ss2;
        ss2 << req1;
        ss1 << req1;
        req1.next_chunk = req1.chunks.size();
        std::string cmp2("16\r\n{\"msg1\":\"Hello World\"}\r\n");
        if(ss2.str() != cmp2){
            return;
        }
        std::string cmp3("POST /run HTTP/1.1\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nAccept: */*\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n16\r\n{\"msg1\":\"Hello World\"}\r\n");
        if(ss1.str() != cmp3){
            return;
        }

        req1.chunks.push_back(
            http::HttpChunk{{0},""}
        );
        std::stringstream ss3;
        ss3 << req1;
        ss1 << req1;
        req1.next_chunk = req1.chunks.size();
        std::string cmp4("0\r\n\r\n");
        std::string cmp5("POST /run HTTP/1.1\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nAccept: */*\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n16\r\n{\"msg1\":\"Hello World\"}\r\n0\r\n\r\n");
        if(ss3.str() != cmp4 || ss1.str() != cmp5){
            return;
        }
        passed_ = true;
    }

    HttpRequestsTests::HttpRequestsTests(ResponseStreamInsertion)
      : passed_{false}
    {
        /* Content Length Insertion. */
        http::HttpResponse res{
            http::HttpVersion::V1_1,
            http::HttpStatus::OK,
            {
                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {{}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        std::stringstream ss;
        ss << res;
        res.status_line_finished = true;
        res.next_header = res.headers.size();
        res.next_chunk = res.chunks.size();

        std::string cmp("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 22\r\n\r\n{\"msg\":\"Hello World!\"}");
        if(ss.str() != cmp){
            return;
        }
        /* Chunked Insertion. */
        http::HttpResponse res1{
            http::HttpVersion::V1_1,
            http::HttpStatus::OK,
            {
                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                {http::HttpHeaderField::END_OF_HEADERS, ""}
            },
            {
                {{22}, "{\"msg\":\"Hello World!\"}"}
            }
        };
        std::stringstream ss1;
        ss1 << res1;
        res1.status_line_finished = true;
        res1.next_header = res1.headers.size();
        res1.next_chunk = res1.chunks.size();
        
        std::string cmp1("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n");
        if(ss1.str() != cmp1){
            return;
        }

        res1.chunks.push_back(
            http::HttpChunk{{22},"{\"msg1\":\"Hello World\"}"}
        );
        std::stringstream ss2;
        ss2 << res1;
        ss1 << res1;
        res1.next_chunk = res1.chunks.size();
        std::string cmp2("16\r\n{\"msg1\":\"Hello World\"}\r\n");
        std::string cmp3("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n16\r\n{\"msg1\":\"Hello World\"}\r\n");
        if(ss1.str() != cmp3 || ss2.str() != cmp2){
            return;
        }

        res1.chunks.push_back(
            http::HttpChunk{{0},""}
        );
        std::stringstream ss3;
        ss3 << res1;
        ss1 << res1;
        res1.next_chunk = res1.chunks.size();
        std::string cmp4("0\r\n\r\n");
        std::string cmp5("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n16\r\n{\"msg\":\"Hello World!\"}\r\n16\r\n{\"msg1\":\"Hello World\"}\r\n0\r\n\r\n");
        if(ss1.str() != cmp5 || ss3.str() != cmp4){
            std::cout << ss1.str() << std::endl;
            std::cout << ss3.str() << std::endl;
            return;
        }
        passed_ = true;
    }
}