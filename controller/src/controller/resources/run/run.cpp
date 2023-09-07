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
    Request::Request( boost::json::object& obj )
    {
        boost::json::object& value = obj.at("value").as_object();
        if (!value.contains("execution_context")){
            value_ = boost::json::object(value);
        } else {
            // TODO: construct an execution context request.
            value_ = boost::json::object(value.at("execution_context").as_object().at("value").as_object());
        }
        for ( auto& kvp: obj ){
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

    std::shared_ptr<controller::app::ExecutionContext> handle(Request& req){
        //TODO: check request for a signature that notifies the controller that this execution context already exists on this controller.
        //If the execution context already exists, instead of constructing a new execution context, instead of constructing a new context, just find the context in the list
        //of contexts, and return the pointer.
        //Otherwise, construct a new execution context.
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::Run{});
        for (auto& relation: ctx_ptr->manifest()){
            boost::context::fiber f{
                [&, req, ctx_ptr, relation](boost::context::fiber&& g) {
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
                        const char* __OW_ACTION_BIN = getenv("__OW_ACTION_BIN");
                        if ( __OW_ACTION_BIN == nullptr ){
                            throw "__OW_ACTION_BIN environment variable not set.";
                        }
                        const char* __OW_ACTION_LAUNCHER = getenv("__OW_ACTION_LAUNCHER");
                        if ( __OW_ACTION_LAUNCHER == nullptr ){
                            throw "__OW_ACTION_LAUNCHER environment varible not set.";
                        }
                        std::vector<const char*> argv{__OW_ACTION_BIN, __OW_ACTION_LAUNCHER, relation->path().stem().string().c_str(), relation->key().c_str(), NULL};
                        // Since this happens AFTER the fork, this is thread safe.
                        // fork(2) means that the child process makes a COPY of the parents environment variables.s
                        for ( auto pair: req.env() ){
                            if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0 ){
                                perror("Exporting environment variable failed.");
                            }
                        }
                        execve(__OW_ACTION_BIN, const_cast<char* const*>(argv.data()), environ);
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
                        if(relation->size() == 0){
                            // continue to use params if the relation has no dependencies.
                            params.append("\n");
                        } else {
                            // Override params with emplaced parameters.
                            boost::json::object jv;
                            boost::json::error_code ec;
                            for (auto& dep: *relation){
                                std::string value = dep->acquire_value();
                                dep->release_value();
                                boost::json::object val = boost::json::parse(value, ec).as_object();
                                jv.emplace(dep->key(), val);
                            }
                            params = boost::json::serialize(jv);
                            params.append("\n");
                        }
                        if( write(downstream[1], params.data(), params.size()) == -1 ){
                            perror("Downstream write in the parent process failed.");
                        }
                        std::size_t max_length = 65536;
                        std::size_t value_size = 0;
                        std::string& value = relation->acquire_value();
                        do {
                            value.resize(max_length + value_size);
                            length = read(upstream[0], (value.data() + value_size), max_length);
                            if ( length == -1 ){
                                throw "Upstream read in the parent process failed.";
                            }
                            value_size += length;
                        } while( (value_size % max_length ) == 0 );
                        value.resize(value_size);
                        #ifdef DEBUG
                        std::cout << value << std::endl;
                        #endif
                        relation->release_value();
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
            {
                //Anonymous scope is to ensure that fibers reference is immediately invalidated.
                std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                fibers.push_back(std::move(f));
                ctx_ptr->release_fibers();
            }

        }
        return ctx_ptr;
    }
}//namespace run
}//namespace resources
}//namespace controller