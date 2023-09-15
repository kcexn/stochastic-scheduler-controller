#include "../echo-app/echo.hpp"
#include "../echo-app/utils/common.hpp"
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

// Global Scheduler Signals.
std::shared_ptr<std::mutex> SIGNAL_MTX_PTR;
std::shared_ptr<std::atomic<int> > SIGNAL_PTR;
std::shared_ptr<std::condition_variable> SIGNAL_CV_PTR;

extern "C"{
    void handler(int signum){
        switch(signum){
            case SIGTERM:
                SIGNAL_PTR->fetch_or(echo::Signals::TERMINATE, std::memory_order::memory_order_relaxed);
                SIGNAL_CV_PTR->notify_all();
                break;
            case SIGCHLD:
                int wstatus = 0;
                pid_t pid = 0;
                do{
                    pid = waitpid(0, &wstatus, WNOHANG);
                } while(pid > 0);
                break;
        }
    }
}

int main(int argc, char* argv[])
{
    SIGNAL_MTX_PTR = std::make_shared<std::mutex>();
    SIGNAL_PTR = std::make_shared<std::atomic<int> >();
    SIGNAL_CV_PTR = std::make_shared<std::condition_variable>();

    struct sigaction new_action = {};
    new_action.sa_handler = handler;
    sigemptyset(&(new_action.sa_mask));
    new_action.sa_flags=0;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGCHLD, &new_action, NULL);

    boost::asio::io_context ioc;
    echo::app echo_app(
        ioc, 
        SIGNAL_MTX_PTR,
        SIGNAL_PTR,
        SIGNAL_CV_PTR
    );

    std::unique_lock<std::mutex> lk(*SIGNAL_MTX_PTR);
    SIGNAL_CV_PTR->wait(lk, [&]{ return (SIGNAL_PTR->load(std::memory_order::memory_order_relaxed) & echo::Signals::TERMINATE); });
    lk.unlock();
    return 0;
}