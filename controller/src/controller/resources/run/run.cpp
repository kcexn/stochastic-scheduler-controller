#include "run.hpp"
#include <ctime>
#include <sys/wait.h>

#ifdef DEBUG
#include <iostream>
#endif

namespace controller{
namespace resources{
namespace run{
    // boost::context::fiber handle(Request& req, std::shared_ptr<controller::app::AppMbox>& mbox_ptr){
    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req ){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>();
        boost::context::fiber f{
            [&, req, ctx_ptr](boost::context::fiber&& g){
                boost::json::value jv(boost::json::object_kind);
                jv = req.value();
                std::string params = boost::json::serialize(jv);
                for (;;){
                    g = std::move(g).resume();
                    //Declare two pipes fds
                    int downstream[2] = {};
                    int upstream[2] = {};

                    //syscall return two pipes.
                    if (pipe(downstream) == -1){
                        perror("Downstream pipe failed to open.");
                    }
                    if (pipe(upstream) == -1){
                        perror("Upstream pip failed to open.");
                    }

                    pid_t pid = fork();
                    if ( pid == 0 ){
                        //Child Process.
                        #ifdef DEBUG
                        std::cout << "Child Process about to exec cat." << std::endl;
                        #endif
                        if( close(downstream[1]) == -1 ){
                            perror("closing the downstream write in the child process failed.");
                        }
                        if ( close(upstream[0]) == -1 ){
                            perror("closing the upstream read in the child process failed.");
                        }
                        if (dup2(downstream[0], STDIN_FILENO) == -1){
                            perror("Failed to map the downstream read to STDIN.");
                        }
                        if (dup2(upstream[1], STDOUT_FILENO) == -1){
                            perror("Failed to map the upstream write to STDOUT.");
                        }
                        std::vector<const char*> argv{"/usr/bin/cat", NULL};
                        execve("/usr/bin/cat", const_cast<char* const*>(argv.data()), NULL);
                        exit(1);
                    } else {
                        //Parent Process.
                        if( close(downstream[0]) == -1 ){
                            perror("closing the downstream read in the parent process failed.");
                        }
                        if ( close(upstream[1]) == -1 ){
                            perror("closing the upstream write in the parent process failed.");
                        }
                        if( write(downstream[1], params.data(), params.size()) == -1 ){
                            perror("Downstream write in the parent process failed.");
                        }
                        ctx_ptr->payload().resize(params.size());
                        int length = read(upstream[0], ctx_ptr->payload().data(), params.size());
                        if ( length == -1 ){
                            perror("Upstream read in the parent process failed.");
                        }
                    }
                    if ( close(downstream[1]) == -1 ){
                        perror("closing the downstream write failed.");
                    }
                    if ( close(upstream[0]) == -1 ){
                        perror("closing the upstream read failed.");
                    }
                    int status;
                    pid_t child_returned = waitpid( pid, &status, WNOHANG);
                    switch (child_returned){
                        case 0:
                            // Child Process with specified PID exists, but has not exited normally yet.
                            // At this stage in the thread though, the output from the process has already been collected.
                            // The only reason we would be in this state is if the application is hanging for some reason.
                            // For example, `cat' glues STDIN to STDOUT and stays running until it receives a signal.
                            // The scheduler thread at this stage will send a SIGTERM to the child process, and performa a blocking wait.
                            if ( kill(pid, SIGTERM) == -1 ){
                                perror("child process failed to terminate.");
                            }
                            if ( waitpid( pid, &status, 0) == -1 ){
                                perror("Wait on child has failed.");
                            }
                            break;
                        case -1:
                            perror("Wait on child pid failed.");
                            break;
                    }
                }
                return std::move(g);
            }
        };
        // Initialize the execution context.
        ctx_ptr->fiber() = std::move(f).resume();
        return ctx_ptr;
    }
}//namespace run
}//namespace resources
}//namespace controller