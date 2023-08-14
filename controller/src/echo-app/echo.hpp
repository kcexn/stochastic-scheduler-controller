#ifndef ECHO_APP_HPP
#define ECHP_APP_HPP
#include "utils/common.hpp"
#include "components/echo-scheduler.hpp"

namespace echo{
    class app
    {
    public:
        app(boost::asio::io_context& ioc, short port);

    private:
        echo::Scheduler echo_scheduler_;
    };
}//echo namespace

#endif