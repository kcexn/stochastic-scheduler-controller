#include "run.hpp"
#include "../../app/action-relation.hpp"
#include "../../app/execution-context.hpp"
#include <csignal>


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
            boost::json::object& context = value.at("execution_context").as_object();
            std::stringstream uuid(std::string(context.at("uuid").as_string()));
            uuid >> execution_context_id_;
            #ifdef DEBUG
            std::cout << execution_context_id_ << std::endl;
            #endif
            boost::json::value& idx = context.at("idx");
            if (idx.is_int64()){
                execution_context_idx_ = idx.get_int64();
            } else if (idx.is_uint64()){
                execution_context_idx_ = idx.get_uint64();
            } else {
                throw "execution context index is too large.";
            }
            #ifdef DEBUG
            std::cout << execution_context_idx_ << std::endl;
            #endif

            value_ = boost::json::object(value.at("execution_context").as_object().at("value").as_object());
            #ifdef DEBUG
            std::cout << value_ << std::endl;
            #endif
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

    std::shared_ptr<controller::app::ExecutionContext> handle(Request& req, std::vector<std::shared_ptr<controller::app::ExecutionContext> >& ctx_ptrs){
        //TODO: check request for a signature that notifies the controller that this execution context already exists on this controller.
        //If the execution context already exists, instead of constructing a new execution context, instead of constructing a new context, just find the context in the list
        //of contexts, and return the pointer.
        //Otherwise, construct a new execution context.
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr;
        if(req.execution_context_id() != UUID::Uuid()){
            #ifdef DEBUG
            std::cout << "Request Context index: " << req.idx() << std::endl;
            std::cout << "Request Execution Context ID: " << req.execution_context_id() << std::endl;
            #endif
            auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                return (ctx_ptr->execution_context_id() == req.execution_context_id());
            });
            if( it != ctx_ptrs.end()){
                #ifdef DEBUG
                std::cout << "Iterator does not equal end()." << std::endl;
                #endif
                (*it)->push_execution_idx(req.idx());
                return std::shared_ptr<controller::app::ExecutionContext>(*it);
            }else{
                ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::run, req.execution_context_id());
            }
        } else {
            ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::run);
        }
        for (auto& relation: ctx_ptr->manifest()){
            boost::context::fiber f{
                [&, req, ctx_ptr, relation](boost::context::fiber&& g) {
                    // Get the index of the relation.
                    auto it = std::find(ctx_ptr->manifest().begin(), ctx_ptr->manifest().end(), relation);
                    std::ptrdiff_t idx = it - ctx_ptr->manifest().begin();

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
                    }
                    // Save the PID in the relevant thread control.
                    ctx_ptr->thread_controls()[idx].pid() = pid;

                    //Parent Process.
                    if( close(downstream[0]) == -1 ){
                        perror("closing the downstream read in the parent process failed.");
                    }
                    if ( close(upstream[1]) == -1 ){
                        perror("closing the upstream write in the parent process failed.");
                    }

                    #ifdef DEBUG
                    std::cout << "Parent process awaiting child: " << pid << ", to notify readiness." << std::endl;
                    #endif

                    char ready[1] = {};
                    int length = read(upstream[0], ready, 1);
                    if( length == -1 ){
                        perror("Upstream read in the parent process failed.");
                    }
                    if (kill(pid, SIGSTOP) == -1){
                        perror("Pausing the child process failed.");
                    }

                    #ifdef DEBUG
                    std::cout << "Child process: " << ", notified parent of readiness, and paused awaiting a future signal." << std::endl;
                    #endif

                    g = std::move(g).resume();

                    #ifdef DEBUG
                    std::cout << "Child process: " << pid << ", to be resumed." << std::endl;
                    #endif
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

                    #ifdef DEBUG
                    std::cout << "Child process: " << pid << ", having params: " << params << std::flush;
                    #endif
                    if( write(downstream[1], params.data(), params.size()) == -1 ){
                        perror("Downstream write in the parent failed");
                    }
                    if (close(downstream[1]) == -1){
                        perror("closing the downstream write failed.");
                    }
                    #ifdef DEBUG
                    std::cout << "Write to child process: " << pid << ", successful." << std::endl;
                    #endif

                    std::size_t max_length = 65536;
                    std::size_t value_size = 0;
                    int errsv = 0;
                    std::string val;         
                    do {
                        errsv = 0;
                        #ifdef DEBUG
                        std::cout << "Start a read from the child process." << std::endl;
                        #endif
                        val.resize(max_length + value_size);
                        length = read(upstream[0], (val.data() + value_size), max_length);
                        if (length != -1){
                            value_size += length;
                        } else {
                            errsv = errno;
                        }
                    } while( (value_size % max_length ) == 0 || errsv == EINTR);
                    if (close(upstream[0]) == -1){
                        perror("closing the upstream read failed.");
                    }
                    val.resize(value_size);
                    #ifdef DEBUG
                    std::cout << val << std::endl;
                    #endif
                    relation->acquire_value() = val;
                    relation->release_value();
                    // waitpid is handled by trapping SIGCHLD
                    return std::move(g);
                }
            };
            ctx_ptr->thread_controls().emplace_back();
            ctx_ptr->thread_controls().back().f() = std::move(f);
        }
        return ctx_ptr;
    }
}//namespace run
}//namespace resources
}//namespace controller