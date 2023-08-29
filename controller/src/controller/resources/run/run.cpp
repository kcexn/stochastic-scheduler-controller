#include "run.hpp"
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <sys/wait.h>

#ifdef DEBUG
#include <iostream>
#endif

namespace controller{
namespace resources{
namespace run{
    Request::Request( boost::json::object obj )
      : value_(obj["value"].as_object())
    {
        for ( auto kvp: obj ){
            std::string key(kvp.key());
            if ( key != "value" ){
                std::string val;
                if ( kvp.value().is_string() ){
                    val = std::string( kvp.value().get_string() );
                } else if ( kvp.value().is_int64() ){
                    val = std::to_string( kvp.value().get_int64() );
                } else if ( kvp.value().is_uint64() ){
                    val = std::to_string( kvp.value().get_uint64() );
                } else if ( kvp.value().is_double() ){
                    val = std::to_string( kvp.value().get_double() );
                }
                std::string envkey("__OW_");
                std::transform(key.cbegin(), key.cend(), key.begin(), []( unsigned char c ) { return std::toupper(c); });
                envkey.append(key);
                #ifdef DEBUG
                std::cout << envkey << "=" << val << std::endl;
                #endif
                env_.emplace(envkey, val);
            }
        }
    }

    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>();
        boost::context::fiber f{
            [&, req, ctx_ptr](boost::context::fiber&& g) {
                boost::json::value jv;
                jv = req.value();
                std::string params = boost::json::serialize(jv);
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
                    std::vector<const char*> argv{"/usr/bin/python3", "-OO", "/workspaces/whisk-controller-dev/action-runtimes/python3/launcher/launcher.py", "fn_000", NULL};

                    // Since this happens AFTER the fork, this is thread safe.
                    // fork(2) means that the child process makes a COPY of the parents environment variables.s
                    for ( auto pair: req.env() ){
                        if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0 ){
                            perror("Exporting environment variable failed.");
                        }
                    }
                    execve("/usr/bin/python3", const_cast<char* const*>(argv.data()), environ);
                    exit(1);
                } else {
                    //Parent Process.
                    if( close(downstream[0]) == -1 ){
                        perror("closing the downstream read in the parent process failed.");
                    }
                    if ( close(upstream[1]) == -1 ){
                        perror("closing the upstream write in the parent process failed.");
                    }

                    char ready[1] = {};
                    int length = read(upstream[0], ready, 1);
                    if( length == -1 ){
                        perror("Upstream read in the parent process failed.");
                    }
                    if (kill(pid, SIGSTOP) == -1){
                        perror("Pausing the child process failed.");
                    }
                    g = std::move(g).resume();
                    if (kill(pid, SIGCONT) == -1 ){
                        perror("Parent process failed to unpause child process.");
                    }

                    params.append("\n");
                    if( write(downstream[1], params.data(), params.size()) == -1 ){
                        perror("Downstream write in the parent process failed.");
                    }
                    int max_length = 65536;
                    ctx_ptr->payload().resize(max_length);
                    length = read(upstream[0], ctx_ptr->payload().data(), max_length);
                    if ( length == -1 ){
                        perror("Upstream read in the parent process failed.");
                    }
                    ctx_ptr->payload().resize(length);
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
                return std::move(g);
            }
        };
        // Initialize the execution context.
        ctx_ptr->fiber() = std::move(f);
        return ctx_ptr;
    }
}//namespace run
}//namespace resources
}//namespace controller