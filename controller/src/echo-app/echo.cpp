#include "echo.hpp"
#include "components/echo-reader.hpp"
#include "components/echo-writer.hpp"
#include "components/echo-worker.hpp"

#ifdef DEBUG
#include <iostream>
#endif

echo::app::app(boost::asio::io_context& ioc, short port)
    :   echo_scheduler_(ioc, port)
{
    #ifdef DEBUG
    std::cout << "App constructor!" << std::endl;
    #endif
    echo_scheduler_.start();
}