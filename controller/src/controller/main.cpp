#include "app/controller-app.hpp"
#include "controller-events.hpp"
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <charconv>

// Global Scheduler Signals.
std::shared_ptr<std::mutex> SIGNAL_MTX_PTR;
std::shared_ptr<std::atomic<std::uint16_t> > SIGNAL_PTR;
std::shared_ptr<std::condition_variable> SIGNAL_CV_PTR;

extern "C"{
    void handler(int signum){
        switch(signum){
            case SIGTERM:
            {
                SIGNAL_PTR->fetch_or(CTL_TERMINATE_EVENT, std::memory_order::memory_order_relaxed);
                SIGNAL_CV_PTR->notify_all();
                break;
            }
            case SIGCHLD:
            {
                int wstatus = 0;
                pid_t pid = 0;
                do{
                    pid = waitpid(-1, &wstatus, WNOHANG);
                } while(pid > 0);
                break;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    int opt;
    const char* port = nullptr;
    const char* usock_path = nullptr;
    while((opt = getopt(argc, argv, "u:p:")) != -1){
        switch(opt)
        {
            case 'u':
                usock_path = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            default:
                std::cerr << "Usage: controller [-u unix_socket_path] [-p sctp_port_number]" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
    if(port == nullptr){
        port = "5100";
    }
    if(usock_path == nullptr){
        usock_path = "/run/controller/controller.sock";
    }
    std::filesystem::path upath(usock_path);
    std::uint16_t sport;
    std::string_view sport_str(port);
    std::from_chars_result fcres = std::from_chars(sport_str.data(), sport_str.data()+sport_str.size(), sport, 10);
    if(fcres.ec != std::errc()){
        std::cerr << std::make_error_code(fcres.ec).message() << std::endl;
        throw "This should never happen.";
    }

    SIGNAL_MTX_PTR = std::make_shared<std::mutex>();
    SIGNAL_PTR = std::make_shared<std::atomic<std::uint16_t> >();
    SIGNAL_CV_PTR = std::make_shared<std::condition_variable>();

    struct sigaction new_action = {};
    new_action.sa_handler = handler;
    sigemptyset(&(new_action.sa_mask));
    new_action.sa_flags=0;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGCHLD, &new_action, NULL);

    sigset_t sigmask = {};
    int status = sigemptyset(&sigmask);
    if(status == -1){
        std::cerr << "controller-app.cpp:147:sigemptyset failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    status = sigaddset(&sigmask, SIGCHLD);
    if(status == -1){
        std::cerr << "controller-app.cpp:152:sigaddmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    status = sigprocmask(SIG_BLOCK, &sigmask, nullptr);
    if(status == -1){
        std::cerr << "controller-app.cpp:157:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    
    boost::asio::io_context ioc;
    std::shared_ptr<controller::io::MessageBox> mbx = std::make_shared<controller::io::MessageBox>();
    mbx->sched_signal_mtx_ptr = SIGNAL_MTX_PTR;
    mbx->sched_signal_ptr = SIGNAL_PTR;
    mbx->sched_signal_cv_ptr = SIGNAL_CV_PTR;
    controller::app::Controller controller(
        mbx,
        ioc,
        upath,
        sport
    );
    std::unique_lock<std::mutex> lk(*SIGNAL_MTX_PTR);
    SIGNAL_CV_PTR->wait(lk, [&]{ return (SIGNAL_PTR->load(std::memory_order::memory_order_relaxed) & CTL_TERMINATE_EVENT); });
    lk.unlock();
    // std::cout << "main.cpp:112:application exited normally." << std::endl;
    return 0;
}