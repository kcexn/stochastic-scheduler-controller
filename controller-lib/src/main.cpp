#include "../tests/uuid/uuid-tests.hpp"
#include "../tests/transport-servers/unix-server/server-tests.hpp"
#include "../tests/application-servers/http/http-requests-tests.hpp"
#include <filesystem>

int main(int argc, char* argv[]){
    {
        // UUID tests.
        tests::Uuid default_uuid;
        if (default_uuid){
            std::cout << "default uuid tests passed." << std::endl;
        } else {
            std::cerr << "Default uuid tests failed." << std::endl;
        }
    }
    {
        // UNIX sockets tests.
        boost::asio::io_context ioc;
        std::filesystem::path p("/run/controller/controller.sock");
        boost::asio::local::stream_protocol::endpoint endpoint(p.string());
        {
            tests::UnixServerTest basic_unix_server(ioc);
            if(basic_unix_server){
                std::cout << "unix server test 1 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 1 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest basic_unix_server(ioc, endpoint);
            if(basic_unix_server){
                std::cout << "unix server test 2 passed." << std::endl;
            } else {
                std::cerr << "unix server test 2 failed." << std::endl;
            }
            std::filesystem::remove(p);
        }
        {
            server::Server server(ioc);
            tests::UnixSessionTest basic_unix_session(ioc, server);
            if(basic_unix_session){
                std::cout << "Unix Session test 1 passed." << std::endl;
            } else {
                std::cerr << "Unix Session test 1 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_push_back(tests::UnixServerTest::test_push_back, ioc);
            if(test_push_back){
                std::cout << "Unix server test 3 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 3 passed." << std::endl;
            }
        }
        {
            server::Server server(ioc);
            boost::asio::local::stream_protocol::socket socket(ioc);
            tests::UnixSessionTest test_move_construct(tests::UnixSessionTest::test_move_construct, std::move(socket), server);
            if(test_move_construct){
                std::cout << "Unix session test 2 passed." << std::endl;
            } else {
                std::cout << "Unix session test 2 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_context_run(tests::UnixServerTest::test_context_run, ioc);
            if(test_context_run){
                std::cout << "Unix server test 4 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 4 failed." << std::endl;
            }
            ioc.restart();
        }
        {
            tests::UnixServerTest test_accept_unix(tests::UnixServerTest::test_accept_unix, ioc, endpoint);
            if(test_accept_unix){
                std::cout << "Unix server test 5 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 5 failed." << std::endl;
            }
            std::filesystem::remove(p);
            ioc.restart();
        }
        {
            tests::UnixServerTest test_read_write(tests::UnixServerTest::test_read_write, ioc, endpoint);
            if(test_read_write){
                std::cout << "Unix server test 6 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 6 failed." << std::endl;
            }
            std::filesystem::remove(p);
            ioc.restart();
        }
        {
            tests::UnixServerTest test_erase_session(tests::UnixServerTest::test_erase_session, ioc, endpoint);
            if(test_erase_session){
                std::cout << "Unix server test 7 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 7 failed." << std::endl;
            }
            std::filesystem::remove(p);
            ioc.restart();
        }
    }
    {
        // HTTP tests.
        {
            tests::HttpRequestsTests test_max_size(tests::HttpRequestsTests::test_max_chunk_size);
            if(test_max_size){
                std::cout << "Http request test 1 passed." << std::endl;
            } else {
                std::cerr << "Http request test 1 failed." << std::endl;
            }
        }
        {
            tests::HttpRequestsTests test_read_chunk(tests::HttpRequestsTests::test_read_chunk);
            if(test_read_chunk){
                std::cout << "Http request test 2 passed." << std::endl;
            } else {
                std::cerr << "Http request test 2 failed." << std::endl;
            }
        }
    }
    return 0;
}