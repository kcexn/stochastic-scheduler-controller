#include "app/controller-app.hpp"
#include "controller-events.hpp"
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <charconv>

volatile bool TERMINATED_ = false;

extern "C"{
    void handler(int signum){
        switch(signum){
            case SIGTERM:
            {
                TERMINATED_ = true;
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
    
    struct sigaction new_action = {};
    new_action.sa_handler = handler;
    sigemptyset(&(new_action.sa_mask));
    new_action.sa_flags=0;
    sigaction(SIGTERM, &new_action, NULL);
    
    boost::asio::io_context ioc;
    std::shared_ptr<controller::io::MessageBox> mbx = std::make_shared<controller::io::MessageBox>();
    sigset_t sigmask = {};
    int status = sigemptyset(&sigmask);
    if(status == -1){
        std::cerr << "controller-app.cpp:78:sigemptyset failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    status = sigaddset(&sigmask, SIGTERM);
    if(status == -1){
        std::cerr << "controller-app.cpp:83:sigaddmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    status = sigaddset(&sigmask, SIGCHLD);
    if(status == -1){
        std::cerr << "controller-app.cpp:83:sigaddmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    status = sigprocmask(SIG_BLOCK, &sigmask, nullptr);
    if(status == -1){
        std::cerr << "controller-app.cpp:88:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    controller::app::Controller controller(
        mbx,
        ioc,
        upath,
        sport
    );
    status = sigdelset(&sigmask, SIGCHLD);
     if(status == -1){
        std::cerr << "controller-app.cpp:88:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }   
    status = sigprocmask(SIG_UNBLOCK, &sigmask, nullptr);
    if(status == -1){
        std::cerr << "controller-app.cpp:99:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "What?";
    }

    struct timespec ts = {1,0};
    while(!TERMINATED_){
        nanosleep(&ts, nullptr);
    }
    return 0;
}