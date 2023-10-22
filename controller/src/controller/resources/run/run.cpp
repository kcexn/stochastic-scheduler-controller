#include "run.hpp"
#include "../../app/action-relation.hpp"
#include "../../app/execution-context.hpp"
#include <csignal>
#include <boost/asio.hpp>

namespace controller{
namespace resources{
namespace run{
    Request::Request( boost::json::object& obj )
    {
        /*{"value":{"execution_context":{"uuid":"a70ea480860c45e19a5385c68188d1ff","idx":0,"peers":[],"value":{}}},
        "namespace":"guest","action_name":"test","api_host":"localhost","api_key":"akey","activation_id":"activation","transaction_id":"transaction","deadline":123456789}*/
        boost::json::object& value = obj.at("value").as_object();
        if (!value.contains("execution_context")){
            value_ = boost::json::object(value);
        } else {
            boost::json::object& context = value.at("execution_context").as_object();
            std::stringstream uuid(std::string(context.at("uuid").as_string()));
            uuid >> execution_context_id_;
            boost::json::value& idx = context.at("idx");
            if (idx.is_int64()){
                execution_context_idx_ = idx.get_int64();
            } else if (idx.is_uint64()){
                execution_context_idx_ = idx.get_uint64();
            } else {
                std::cerr << "run.cpp:31:execution context index is too large." << std::endl;
                throw "execution context index is too large.";
            }
            boost::json::array& peers = context.at("peers").as_array();
            for(auto& peer: peers){
                peers_.emplace_back(peer.as_string());
            }
            value_ = boost::json::object(context.at("value").as_object());
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
                env_.emplace(envkey, val);
            }
        }
    }

    std::shared_ptr<controller::app::ExecutionContext> handle(Request& req, std::vector<std::shared_ptr<controller::app::ExecutionContext> >& ctx_ptrs, boost::asio::io_context& ioc){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr;
        if(req.execution_context_id() != UUID::Uuid()){
            auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                return (ctx_ptr->execution_context_id() == req.execution_context_id());
            });
            if( it != ctx_ptrs.end()){
                (*it)->execution_context_idx_array().push_back(req.idx());
                (*it)->push_execution_idx(req.idx());
                return std::shared_ptr<controller::app::ExecutionContext>(*it);
            }else{
                ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::run, req.execution_context_id(), req.idx(), req.peers());
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
                        std::cerr << "Downstream pipe filed to open: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "this shouldn't happen!";
                    }
                    if (pipe(upstream) == -1){
                        std::cerr << "Upstream pipe failed to open: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "this shouldn't happen!";
                    }
                    // Declare a synchronization pipe.
                    int sync[2] = {};
                    if(pipe(sync) == -1){
                        std::cerr << "Synchronization pipe failed to open: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "this shouldn't happen!";
                    }
                    const char* __OW_ACTION_BIN = getenv("__OW_ACTION_BIN");
                    if ( __OW_ACTION_BIN == nullptr ){
                        std::cerr << "__OW_ACTION_BIN envvar is not set." << std::endl;
                        throw "__OW_ACTION_BIN environment variable not set.";
                    }
                    const char* __OW_ACTION_LAUNCHER = getenv("__OW_ACTION_LAUNCHER");
                    if ( __OW_ACTION_LAUNCHER == nullptr ){
                        std::cerr << "__OW_ACTION_LAUNCHER envvar is not set." << std::endl;
                        throw "__OW_ACTION_LAUNCHER environment varible not set.";
                    }
                    std::string p = relation->path().stem().string();
                    std::string k = relation->key();
                    std::vector<const char*> argv{__OW_ACTION_BIN, __OW_ACTION_LAUNCHER, p.c_str(), k.c_str(), nullptr};
                    pid_t pid = fork();
                    switch(pid){
                        case 0:
                        {
                            //Child Process.
                            errno = 0;
                            int status = nice(2);
                            if(status == -1 && errno != 0){
                                std::cerr << "controller-io.cpp:141:nice failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            }
                            setpgid(0,0);
                            char notice[1] = {'\0'};
                            if(close(sync[0]) == -1){
                                std::cerr << "Closing the sync read side in the child process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            int count = 0;
                            while(true){
                                count = write(sync[1], notice, 1);
                                if(count == -1 && errno != EINTR){
                                    std::cerr << "Writing to the synchronization pipe failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                    throw "This shouldn't happen.";
                                } else if ( count >= 0 ){
                                    break;
                                }
                            }
                            if( close(sync[1]) == -1 ){
                                std::cerr << "Closing the write side of the synchronization pipe in the child failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            if( close(downstream[1]) == -1 ){
                                std::cerr << "Closing the downstream write in the child process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            if ( close(upstream[0]) == -1 ){
                                std::cerr << "Closing the upstream read in the child process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            if (dup2(downstream[0], STDIN_FILENO) == -1){
                                std::cerr << "Failed to map the downstream read to STDIN: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            if (dup2(upstream[1], 3) == -1){
                                std::cerr << "Failed to map the upstream write to fd3: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "This shouldn't happen.";
                            }
                            // Since this happens AFTER the fork, this is thread safe.
                            // fork(2) means that the child process makes a COPY of the parents environment variables.s
                            for ( auto pair: req.env() ){
                                if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0 ){
                                    std::cerr << "Exporting environment variable failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                }
                            }
                            execve(__OW_ACTION_BIN, const_cast<char* const*>(argv.data()), environ);
                            exit(1);
                        }
                        case -1:
                        {
                            // Error
                            std::cerr << "Fork failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                            throw "This shouldn't happen.";
                        }
                    }
                    // Block until the child process sets the pgid.
                    char notice[1] = {};
                    int count = 0;
                    do{
                        count = read(sync[0], notice, 1);
                        if(count < 0 && errno != EINTR && errno != EAGAIN){
                            std::cerr << "run.cpp:193:read from the synchronization pipe failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            throw "This shouldn't happen.";
                        }
                    }while(errno == EINTR || errno == EAGAIN);
                    // Save the PID in the relevant thread control.
                    ctx_ptr->thread_controls()[idx].pid() = pid;    
                    ctx_ptr->synchronize();
                    if(close(sync[1]) == -1){
                        std::cerr << "Closing the write side of the synchronization pipe in the parent process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "This shouldn't happen.";
                    }
                    if(close(sync[0]) == -1){
                        std::cerr << "Closing the read side of the synchronization pipe in the parent failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "This shouldn't happen.";
                    }
                    //Parent Process.
                    if( close(downstream[0]) == -1 ){
                        std::cerr << "Closing the downstream read in the parent process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "this shouldn't happen.";
                    }
                    if ( close(upstream[1]) == -1 ){
                        std::cerr << "Closing the upstream write in the parent process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "this shouldn't happen.";
                    }
                    char ready[1] = {};
                    int length = 0;
                    do{
                        length = read(upstream[0], ready, 1);
                        if(length == -1 && errno != EINTR && errno != EAGAIN){
                            std::cerr << "run.cpp:226:upstream read of the ready byte failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            throw "This shouldn't happen.";
                        }
                    }while(errno == EINTR || errno == EAGAIN);
                    if(kill(-pid, SIGSTOP) == -1){
                        int errsv = errno;
                        struct timespec ts = {};
                        int status = clock_gettime(CLOCK_REALTIME, &ts);
                        if(status == -1){
                            std::cerr << "run.cpp:226:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            std::cerr << "run.cpp:227:pausing the action launcher failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                        } else {
                            std::cerr << "run.cpp:229:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":pausing the action launcher failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                        }
                        switch(errsv)
                        {
                            case ESRCH:
                                return std::move(g);
                            default:
                                throw "what?";
                        }
                    }

                    g = std::move(g).resume();

                    if (kill(-pid, SIGCONT) == -1){
                        int errsv = errno;
                        struct timespec ts = {};
                        int status = clock_gettime(CLOCK_REALTIME, &ts);
                        if(status == -1){
                            std::cerr << "run.cpp:247:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            std::cerr << "run.cpp:248:unpausing the action launcher failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                        } else {
                            std::cerr << "run.cpp:250:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":unpausing the action launcher failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                        }
                        switch(errsv)
                        {
                            case ESRCH:
                                return std::move(g);
                            default:
                                throw "what?";
                        }
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
                    do{
                        length = write(downstream[1], params.data(), params.size());
                        if(length == -1){
                            int errsv = errno;
                            struct timespec ts = {};
                            int status = clock_gettime(CLOCK_REALTIME, &ts);
                            if(status == -1){
                                std::cerr << "run.cpp:326:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                std::cerr << "run.cpp:327:downstream write from the parent process failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                            } else {
                                std::cerr << "run.cpp:329:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":downstream write from the parent process failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                            }
                            switch(errsv)
                            {
                                case EAGAIN:
                                    break;
                                case EINTR:
                                    break;
                                case EBADF:
                                    return std::move(g);
                                case EPIPE:
                                    return std::move(g);
                                default:
                                    throw "what?";
                            }
                        }
                    }while(errno == EAGAIN || errno == EINTR);
                    if (close(downstream[1]) == -1){
                        std::cerr << "Closing the downstream write side of the pipe from the parent process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "This shouldn't happen.";
                    }
                    std::size_t max_length = 65536;
                    std::size_t value_size = 0;
                    std::string val;         
                    do {
                        val.resize(max_length + value_size);
                        length = read(upstream[0], (val.data() + value_size), max_length);
                        if (length != -1){
                            value_size += length;
                        }
                    } while(value_size == val.size() || errno == EINTR || errno == EAGAIN);
                    if (close(upstream[0]) == -1){
                        std::cerr << "Closing the upstream read side of the pipe in the parent process failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "This shouldn't happen.";
                    }
                    val.resize(value_size);
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