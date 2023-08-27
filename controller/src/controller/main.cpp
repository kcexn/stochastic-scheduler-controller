#include "../echo-app/echo.hpp"
// #include "../unix-server/unix-server.hpp"

int main(int argc, char* argv[])
{
    boost::asio::io_context ioc;
    echo::app echo_app(ioc, 5100);
//  From the Boost asio examples:

//   boost::asio::io_context io;
//   printer p(io);
//   boost::thread t(boost::bind(&boost::asio::io_context::run, &io));
//   io.run();
//   t.join();

    return 0;
}