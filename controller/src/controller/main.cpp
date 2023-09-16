#include "app/controller-app.hpp"
#include "controller-events.hpp"
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

// Global Scheduler Signals.
std::shared_ptr<std::mutex> SIGNAL_MTX_PTR;
std::shared_ptr<std::atomic<std::uint16_t> > SIGNAL_PTR;
std::shared_ptr<std::condition_variable> SIGNAL_CV_PTR;

extern "C"{
    void handler(int signum){
        switch(signum){
            case SIGTERM:
                SIGNAL_PTR->fetch_or(CTL_TERMINATE_EVENT, std::memory_order::memory_order_relaxed);
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
    SIGNAL_PTR = std::make_shared<std::atomic<std::uint16_t> >();
    SIGNAL_CV_PTR = std::make_shared<std::condition_variable>();

    struct sigaction new_action = {};
    new_action.sa_handler = handler;
    sigemptyset(&(new_action.sa_mask));
    new_action.sa_flags=0;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGCHLD, &new_action, NULL);

    boost::asio::io_context ioc;
    std::shared_ptr<controller::io::MessageBox> mbx = std::make_shared<controller::io::MessageBox>();
    mbx->sched_signal_mtx_ptr = SIGNAL_MTX_PTR;
    mbx->sched_signal_ptr = SIGNAL_PTR;
    mbx->sched_signal_cv_ptr = SIGNAL_CV_PTR;
    controller::app::Controller controller(
        mbx,
        ioc
    );
    std::unique_lock<std::mutex> lk(*SIGNAL_MTX_PTR);
    SIGNAL_CV_PTR->wait(lk, [&]{ return (SIGNAL_PTR->load(std::memory_order::memory_order_relaxed) & CTL_TERMINATE_EVENT); });
    lk.unlock();
    return 0;
}