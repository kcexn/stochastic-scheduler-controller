#include "../tests/uuid/uuid-tests.hpp"
#include "../tests/transport-servers/unix-server/server-tests.hpp"
#include <filesystem>

int main(int argc, char* argv[]){
    {
        tests::Uuid default_uuid;
        if (default_uuid){
            std::cout << "default uuid tests passed." << std::endl;
        } else {
            std::cerr << "Default uuid tests failed." << std::endl;
        }
    }
    {
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
            tests::UnixSessionTest basic_unix_session(ioc);
            if(basic_unix_session){
                std::cout << "Unix Session test 1 passed." << std::endl;
            } else {
                std::cerr << "Unix Session test 1 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_emplace_back(tests::UnixServerTest::test_emplace_back, ioc);
            if(test_emplace_back){
                std::cout << "Unix server test 3 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 3 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_push_back(tests::UnixServerTest::test_push_back, ioc);
            if(test_push_back){
                std::cout << "Unix server test 4 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 4 passed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_push_and_emplace_back(tests::UnixServerTest::test_push_and_emplace_back, ioc);
            if(test_push_and_emplace_back){
                std::cout << "Unix server test 5 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 5 passed." << std::endl;
            }
        }
        {
            boost::asio::local::stream_protocol::socket socket(ioc);
            tests::UnixSessionTest test_move_construct(tests::UnixSessionTest::test_move_construct, std::move(socket));
            if(test_move_construct){
                std::cout << "Unix session test 2 passed." << std::endl;
            } else {
                std::cout << "Unix session test 2 failed." << std::endl;
            }
        }
        {
            tests::UnixServerTest test_context_run(tests::UnixServerTest::test_context_run, ioc);
            if(test_context_run){
                std::cout << "Unix server test 6 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 6 failed." << std::endl;
            }
            ioc.restart();
        }
        {
            tests::UnixServerTest test_accept_unix(tests::UnixServerTest::test_accept_unix, ioc, endpoint);
            if(test_accept_unix){
                std::cout << "Unix server test 7 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 7 failed." << std::endl;
            }
            std::filesystem::remove(p);
            ioc.restart();
        }
        {
            tests::UnixServerTest test_read_write(tests::UnixServerTest::test_read_write, ioc, endpoint);
            if(test_read_write){
                std::cout << "Unix server test 8 passed." << std::endl;
            } else {
                std::cerr << "Unix server test 8 failed." << std::endl;
            }
            std::filesystem::remove(p);
            ioc.restart();
        }
    }

    
    

    

    // UUID::Uuid uuid(UUID::Uuid::v4);
    // std::stringstream ss;
    // ss << uuid;
    // std::cout << uuid << std::endl;

    // UUID::Uuid uuid1;
    // ss >> uuid1;
    // std::cout << uuid1 << std::endl;

    // std::cout << std::boolalpha << (uuid == uuid1) << std::endl;
    // UUID::Uuid uuid2(UUID::Uuid::v4);
    // std::cout << std::boolalpha << (uuid1 == uuid2) << std::endl;

    return 0;
}