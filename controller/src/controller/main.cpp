#include "echo.hpp"

int main(int argc, char* argv[])
{
    boost::asio::io_context ioc;
    echo::app echo_app(ioc, 5100);
    return 0;
}