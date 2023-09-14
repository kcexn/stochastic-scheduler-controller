#include "sctp-server-tests.hpp"
#include <iostream>
namespace tests{
    SctpServerTests::SctpServerTests(DefaultConstructor, boost::asio::io_context& ioc)
    {
        sctp_transport::SctpServer sctp_server(ioc);
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(SocketConstructor, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc, endpoint);
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(TestSocketRead, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc,endpoint);
        sctp_server.init([&](const boost::system::error_code& ec){
            std::cout << "New Read!" << std::endl;
        });
        ioc.run_for(std::chrono::duration<int>(5));
        // sctp_server.stop();
        // struct timespec ts = {5, 0};
        // nanosleep(&ts, 0);
        passed_ = true;
    }
}