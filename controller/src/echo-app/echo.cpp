#include "echo.hpp"
#include "components/echo-reader.hpp"
#include "components/echo-writer.hpp"
#include "components/echo-worker.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace echo{
    app::app(
        boost::asio::io_context& ioc, 
        short port,
        std::shared_ptr<std::mutex> signal_mtx_ptr,
        std::shared_ptr<std::atomic<int> > signal_ptr,
        std::shared_ptr<std::condition_variable> signal_cv_ptr
    )
        :   echo_scheduler_(ioc, port, signal_mtx_ptr, signal_ptr, signal_cv_ptr)
    {
        #ifdef DEBUG
        std::cout << "App constructor!" << std::endl;
        #endif
        echo_scheduler_.start();
    }
}//namespace echo