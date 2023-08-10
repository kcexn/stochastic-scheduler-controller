#include "echo.hpp"

int main(int argc, char* argv[])
{
    boost::asio::io_context ioc;

    //More than one application can be registered onto the io-context.
    echo::app echo_app(ioc, 5100);
    // ioc.run();
    return 0;
}