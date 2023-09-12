#include "../tests/uuid/uuid-tests.hpp"
#include "../tests/transport-servers/unix-server/server-tests.hpp"
#include "../tests/application-servers/http/http-requests-tests.hpp"
#include "../tests/application-servers/http/http-server-tests.hpp"
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
    //     boost::asio::io_context ioc;
    //     std::filesystem::path p("/run/controller/controller.sock");
    //     boost::asio::local::stream_protocol::endpoint endpoint(p.string());
    //     {
    //         tests::UnixServerTest basic_unix_server(ioc);
    //         if(basic_unix_server){
    //             std::cout << "unix server test 1 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 1 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::UnixServerTest basic_unix_server(ioc, endpoint);
    //         if(basic_unix_server){
    //             std::cout << "unix server test 2 passed." << std::endl;
    //         } else {
    //             std::cerr << "unix server test 2 failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //     }
    //     {
    //         server::Server server(ioc);
    //         tests::UnixSessionTest basic_unix_session(ioc, server);
    //         if(basic_unix_session){
    //             std::cout << "Unix Session test 1 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix Session test 1 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::UnixServerTest test_push_back(tests::UnixServerTest::test_push_back, ioc);
    //         if(test_push_back){
    //             std::cout << "Unix server test 3 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 3 passed." << std::endl;
    //         }
    //     }
    //     {
    //         server::Server server(ioc);
    //         boost::asio::local::stream_protocol::socket socket(ioc);
    //         tests::UnixSessionTest test_move_construct(tests::UnixSessionTest::test_move_construct, std::move(socket), server);
    //         if(test_move_construct){
    //             std::cout << "Unix session test 2 passed." << std::endl;
    //         } else {
    //             std::cout << "Unix session test 2 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::UnixServerTest test_context_run(tests::UnixServerTest::test_context_run, ioc);
    //         if(test_context_run){
    //             std::cout << "Unix server test 4 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 4 failed." << std::endl;
    //         }
    //         ioc.restart();
    //     }
    //     {
    //         tests::UnixServerTest test_accept_unix(tests::UnixServerTest::test_accept_unix, ioc, endpoint);
    //         if(test_accept_unix){
    //             std::cout << "Unix server test 5 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 5 failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //     }
    //     {
    //         tests::UnixServerTest test_read_write(tests::UnixServerTest::test_read_write, ioc, endpoint);
    //         if(test_read_write){
    //             std::cout << "Unix server test 6 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 6 failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //     }
    //     {
    //         tests::UnixServerTest test_erase_session(tests::UnixServerTest::test_erase_session, ioc, endpoint);
    //         if(test_erase_session){
    //             std::cout << "Unix server test 7 passed." << std::endl;
    //         } else {
    //             std::cerr << "Unix server test 7 failed." << std::endl;
    //         }
    //         std::filesystem::remove(p);
    //         ioc.restart();
    //     }
    // }
    // {
    //     // HTTP tests.
    //     {
    //         tests::HttpRequestsTests test_big_num(tests::HttpRequestsTests::test_big_num);
    //         if(test_big_num){
    //             std::cout << "Http request test 1 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 1 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_read_chunk(tests::HttpRequestsTests::test_read_chunk);
    //         if(test_read_chunk){
    //             std::cout << "Http request test 2 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 2 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_write_chunk(tests::HttpRequestsTests::test_write_chunk);
    //         if(test_write_chunk){
    //             std::cout << "Http request test 3 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 3 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_read_header(tests::HttpRequestsTests::test_read_header);
    //         if(test_read_header){
    //             std::cout << "Http request test 4 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 4 passed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_write_header(tests::HttpRequestsTests::test_write_header);
    //         if(test_write_header){
    //             std::cout << "Http request test 5 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 5 passed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_read_request(tests::HttpRequestsTests::test_read_request);
    //         if(test_read_request){
    //             std::cout << "Http request test 6 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 6 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_write_request(tests::HttpRequestsTests::test_write_request);
    //         if(test_write_request){
    //             std::cout << "Http request test 7 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 7 failed." << std::endl;
    //         }
    //     }
    //     {
    //         tests::HttpRequestsTests test_write_response(tests::HttpRequestsTests::test_write_response);
    //         if(test_write_response){
    //             std::cout << "Http requests test 8 passed." << std::endl;
    //         } else {
    //             std::cerr << "Http request test 8 failed." << std::endl;
    //         }
    //     }
    // }
    {
        //Http Server Tests.
        std::size_t test_num = 1;
        {
            tests::HttpServerTests test_construct_server(tests::HttpServerTests::test_construct_server);
            if(test_construct_server){
                std::cout << "Http server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Http server test " << test_num << " failed." << std::endl;
            }
            ++test_num;
        }
        {
            tests::HttpServerTests test_construct_session(tests::HttpServerTests::test_construct_session);
            if(test_construct_session){
                std::cout << "Http server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Http server test " << test_num << " passed." << std::endl;
            }
            ++test_num;
        }
        {
            tests::HttpServerTests test_construct_unix_bound_session(tests::HttpServerTests::test_construct_unix_bound_session);
            if(test_construct_unix_bound_session){
                std::cout << "Http server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Http server test " << test_num << " passed." << std::endl;
            }
            ++test_num;
        }
        {
            tests::HttpServerTests test_unix_bound_read_write(tests::HttpServerTests::test_unix_bound_read_write);
            if(test_unix_bound_read_write){
                std::cout << "Http server test " << test_num << " passed." << std::endl;
            } else {
                std::cerr << "Http server test " << test_num << " passed." << std::endl;
            }
            ++test_num;
        }
    }
    return 0;
}