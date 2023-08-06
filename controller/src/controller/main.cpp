#include <iostream>
#include <boost/asio.hpp>
#include "sctp.hpp"
#include "sctp-server.hpp"

int main(int argc, char* argv[])
{
    try
    {
        boost::asio::io_context ioc;
        sctp_server s(ioc, 5100);
        ioc.run();
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}