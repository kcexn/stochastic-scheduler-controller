#ifndef ECHO_APP_HPP
#define ECHP_APP_HPP
#include "utils/common.hpp"
#include "components/echo-scheduler.hpp"

namespace echo{
    class app
    {
    public:
        app(
            boost::asio::io_context& ioc, 
            short port,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );

    private:
        echo::Scheduler echo_scheduler_;
    };
}//echo namespace

#endif