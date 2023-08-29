#include "../echo-app/echo.hpp"
#include <csignal>
#include <unistd.h>

// Global Scheduler Signals.
std::shared_ptr<std::mutex> SIGNAL_MTX_PTR;
std::shared_ptr<std::atomic<int> > SIGNAL_PTR;
std::shared_ptr<std::condition_variable> SIGNAL_CV_PTR;

extern "C" {
    void sigterm_handler(int signum){
        if (signum == SIGTERM){
            SIGNAL_PTR->fetch_or(echo::Signals::TERMINATE, std::memory_order::memory_order_relaxed);
            SIGNAL_CV_PTR->notify_all();
        }
    }
}

int main(int argc, char* argv[])
{
    SIGNAL_MTX_PTR = std::make_shared<std::mutex>();
    SIGNAL_PTR = std::make_shared<std::atomic<int> >();
    SIGNAL_CV_PTR = std::make_shared<std::condition_variable>();

    struct sigaction new_action = {};
    new_action.sa_handler = sigterm_handler;
    sigemptyset(&(new_action.sa_mask));
    new_action.sa_flags=0;
    sigaction(SIGTERM, &new_action, NULL);

    boost::asio::io_context ioc;
    echo::app echo_app(
        ioc, 
        5100,
        SIGNAL_MTX_PTR,
        SIGNAL_PTR,
        SIGNAL_CV_PTR
    );
    return 0;
}