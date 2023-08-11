#include "echo.hpp"

int main(int argc, char* argv[])
{
    boost::asio::io_context ioc;

    //More than one application can be registered onto the io-context.
    echo::app echo_app(ioc, 5100);

//  From the Boost asio examples:

//   boost::asio::io_context io;
//   printer p(io);
//   boost::thread t(boost::bind(&boost::asio::io_context::run, &io));
//   io.run();
//   t.join();

// Down the line I should consider defining the asynchronous read and write functions as member functions of the scheduler class
// and then dispatch them onto parallel threads by binding the io_context to the run method (roughly equivalent to epoll_create, then epoll_wait).

    return 0;
}