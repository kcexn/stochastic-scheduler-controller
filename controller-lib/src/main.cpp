#include "../tests/uuid/uuid-tests.hpp"
#include "../tests/transport-servers/unix-server/server-tests.hpp"
#include "../tests/application-servers/http/http-requests-tests.hpp"
#include "../tests/application-servers/http/http-server-tests.hpp"
#include "../tests/transport-servers/sctp-server/sctp-server-tests.hpp"
#include <filesystem>

int main(int argc, char* argv[]){
    // {
    //     // UUID tests.
    //     tests::Uuid default_uuid;
    //     if (default_uuid){
    //         std::cout << "default uuid tests passed." << std::endl;
    //     } else {
    //         std::cerr << "Default uuid tests failed." << std::endl;
    //     }
    // }
    // {
    //     // UNIX sockets tests.
    //     using namespace tests;
    //     boost::asio::io_context ioc;
    //     std::filesystem::path p("/run/controller/controller.sock");
    //     std::size_t test_num = 1;
    //     boost::asio::local::stream_protocol::endpoint endpoint(p.string());
    //     {
    //         UnixServerTest basic_unix_server(ioc);
    //         if(basic_unix_server){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest basic_unix_server(ioc, endpoint);
    //         if(basic_unix_server){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_push_back(UnixServerTest::test_push_back, ioc);
    //         if(test_push_back){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_context_run(UnixServerTest::test_context_run, ioc);
    //         if(test_context_run){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         ioc.restart();
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_accept_unix(UnixServerTest::test_accept_unix, ioc, endpoint);
    //         if(test_accept_unix){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_read_write(UnixServerTest::test_read_write, ioc, endpoint);
    //         if(test_read_write){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_erase_session(UnixServerTest::test_erase_session, ioc, endpoint);
    //         if(test_erase_session){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //         ++test_num;
    //     }
    //     {
    //         UnixServerTest test_unix_connect(UnixServerTest::test_unix_connect, ioc, endpoint);
    //         if(test_unix_connect){
    //             std::cout << "Unix server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test " << test_num << " failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //         ++test_num;
    //     }
    // }
    // {
    //     // HTTP tests.
    //     using namespace tests;
    //     std::size_t test_num = 1;
    //     {
    //         HttpRequestsTests test_big_num(HttpRequestsTests::test_big_num);
    //         if(test_big_num){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_read_chunk(HttpRequestsTests::test_read_chunk);
    //         if(test_read_chunk){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_write_chunk(HttpRequestsTests::test_write_chunk);
    //         if(test_write_chunk){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_read_header(HttpRequestsTests::test_read_header);
    //         if(test_read_header){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_write_header(HttpRequestsTests::test_write_header);
    //         if(test_write_header){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_request_stream_extraction(HttpRequestsTests::test_request_stream_extraction);
    //         if(test_request_stream_extraction){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_response_stream_extraction(HttpRequestsTests::test_response_stream_extraction);
    //         if(test_response_stream_extraction){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_request_stream_insertion(HttpRequestsTests::test_request_stream_insertion);
    //         if(test_request_stream_insertion){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpRequestsTests test_response_stream_insertion(HttpRequestsTests::test_response_stream_insertion);
    //         if(test_response_stream_insertion){
    //             std::cout << "Http request test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    // }
    // {
    //     //Http Server Tests.
    //     using namespace tests;
    //     std::size_t test_num = 1;
    //     {
    //         HttpServerTests test_construct_server(HttpServerTests::test_construct_server);
    //         if(test_construct_server){
    //             std::cout << "Http server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpServerTests test_construct_session(HttpServerTests::test_construct_session);
    //         if(test_construct_session){
    //             std::cout << "Http server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpServerTests test_construct_unix_bound_session(HttpServerTests::test_construct_unix_bound_session);
    //         if(test_construct_unix_bound_session){
    //             std::cout << "Http server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpServerTests test_unix_bound_read_write(HttpServerTests::test_unix_bound_read_write);
    //         if(test_unix_bound_read_write){
    //             std::cout << "Http server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    //     {
    //         HttpServerTests test_unix_client_write(HttpServerTests::test_unix_client_write);
    //         if(test_unix_client_write){
    //             std::cout << "Http server test " << test_num << " passed." << std::endl;
    //         } else {
    //             std::cerr << "Http server test " << test_num << " failed." << std::endl;
    //         }
    //         ++test_num;
    //     }
    // }
    {
        std::size_t test_num = 1;
        {
            using namespace tests;
            boost::asio::io_context ioc;
            transport::protocols::sctp::endpoint endpoint(transport::protocols::sctp::v4(), 5100);
            SctpServerTests test_constructor(SctpServerTests::test_constructor, ioc);
            if(test_constructor){
                std::cout << "Sctp server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Sctp server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
        {
            using namespace tests;
            boost::asio::io_context ioc;
            transport::protocols::sctp::endpoint endpoint(transport::protocols::sctp::v4(), 5100);
            SctpServerTests test_session_constructor(SctpServerTests::test_session_constructor, ioc, endpoint);
            if(test_session_constructor){
                std::cout << "Sctp server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Sctp server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
        {
            using namespace tests;
            boost::asio::io_context ioc;
            transport::protocols::sctp::endpoint endpoint(transport::protocols::sctp::v4(), 5100);
            SctpServerTests test_socket_read(SctpServerTests::test_socket_read, ioc, endpoint);
            if(test_socket_read){
                std::cout << "Sctp server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Sctp server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
        {
            using namespace tests;
            boost::asio::io_context ioc;
            transport::protocols::sctp::endpoint endpoint(transport::protocols::sctp::v4(), 5100);
            SctpServerTests test_session_read_write(SctpServerTests::test_session_read_write, ioc, endpoint);
            if(test_session_read_write){
                std::cout << "Sctp server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Sctp server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
        {
            using namespace tests;
            boost::asio::io_context ioc;
            transport::protocols::sctp::endpoint endpoint(transport::protocols::sctp::v4(), 5100);
            SctpServerTests test_session_connect(SctpServerTests::test_session_connect, ioc, endpoint);
            if(test_session_connect){
                std::cout << "Sctp server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Sctp server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
    }
    return 0;
}