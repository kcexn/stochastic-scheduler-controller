#include "thread-controls.hpp"
#include <csignal>
#include <iostream>
#include <sys/resource.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>

#define MAX_LENGTH 65535

static void subprocess(int* downstream, int* upstream, int efd, std::vector<const char*>& argv, std::map<std::string, std::string>& env) {
    int len = 0;
    std::uint64_t notice = 1;
    std::size_t bytes_written = 0;
    if(setpgid(0,0) == -1){
        std::cerr << "thread-controls.cpp:14:setpgid failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    do{
        len = write(efd, &notice, sizeof(notice));
        if(len == -1){
            switch(errno)
            {
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:25:write() failed." << std::endl;
                    throw "what?";
            }
        } else if (bytes_written < sizeof(notice)){
            bytes_written += len;
        } else {
            break;
        }
    }while(bytes_written < sizeof(notice));

    if(close(efd) == -1){
        std::cerr << "thread-controls.cpp:36:close(efd) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    if(close(downstream[1]) == -1){
        std::cerr << "thread-controls.cpp:40:close(downstream[1]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "This shouldn't happen.";
    }
    if(close(upstream[0]) == -1){
        std::cerr << "thread-controls.cpp:44:close(downstream[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "This shouldn't happen.";
    }
    if (dup2(downstream[0], STDIN_FILENO) == -1){
        std::cerr << "thread-controls.cpp:48:dup2(downstream[0], STDIN) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "This shouldn't happen.";
    }
    if (dup2(upstream[1], 3) == -1){
        std::cerr << "thread-controls.cpp:52:dup2(upstream[1], 3) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "This shouldn't happen.";
    }
    const char* __OW_ACTION_BIN = getenv("__OW_ACTION_BIN");
    if(__OW_ACTION_BIN == nullptr){
        std::cerr << "thread-controls.cpp:57:__OW_ACTION_BIN envvar not defined.";
        throw "what?";
    }
    for (auto pair: env){
        if ( setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0 ){
            std::cerr << "thread-controls.cpp:57:Exporting environment variable failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
        }
    }
    execve(__OW_ACTION_BIN, const_cast<char* const*>(argv.data()), environ);
    exit(1);
    return; 
}

static bool fork_exec(std::array<int, 2>& pipe_, pid_t& pid_, std::shared_ptr<controller::app::Relation> relation, std::map<std::string, std::string>& env) {
    //Declare two pipes fds
    int downstream[2] = {};
    int upstream[2] = {};
    int len = 0;
    std::uint64_t notice = 0;
    std::size_t bytes_read = 0;
    int efd = 0;
    //syscall return two pipes.
    if (pipe(downstream) == -1){
        std::cerr << "thread-controls.cpp:75:pipe(downstream) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    if (pipe(upstream) == -1){
        std::cerr << "thread-controls.cpp:79:pipe(upstream) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    efd = eventfd(0, EFD_CLOEXEC);
    if(efd == -1){
        switch(errno)
        {
            default:
                std::cerr << "thread-controls.cpp:87:eventfd() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                throw "what?";
        }
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
    switch(pid)
    {
        case 0:
        {
            subprocess(downstream, upstream, efd, argv, env);
            exit(1);
        }
        case -1:
        {
            std::cerr << "thread-controls.cpp:120:fork failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "This shouldn't happen.";
        }
        default:
            break;
    }
    do{
        len = read(efd, &notice, sizeof(notice));
        if(len < 0){
            switch(errno)
            {
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:134:read(efd) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else if(len == 0){
            // len == 0 indicates EOF.
            if(bytes_read < sizeof(notice)){
                std::cerr << "thread-controls.cpp:140:read(efd) encountered eof unexpectedly:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                throw "what?";
            }
        } else {
            bytes_read += len;
        }
    }while(bytes_read < sizeof(notice));   
    pid_ = pid;
    if(close(efd) == -1){
        std::cerr << "thread-controls.cpp:143:close(efd) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    if(close(downstream[0]) == -1){
        std::cerr << "thread-controls.cpp:147:close(downstream[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "this shouldn't happen.";
    }
    if (close(upstream[1]) == -1){
        std::cerr << "thread-controls.cpp:151:close(upstream[1]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "this shouldn't happen.";
    }
    pipe_[0] = upstream[0];
    pipe_[1] = downstream[1];
    return true;
}

static bool wait_for_launcher(std::array<int, 2>& pipe){
    char ready[1] = {};
    int len = 0;
    do {
        len = read(pipe[0], ready, 1);
        if(len < 0){
            switch(errno)
            {
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:76:read(pipe[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            } 
        } else if(len == 0){
            std::cerr << "thread-controls.cpp:80:read(pipe[0]) encountered eof unexepectedly:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        } else {
            return true;
        }
    }while(len < 1);
    return true;
}

static bool subprocess_pause(pid_t pid){
    if(kill(-pid, SIGSTOP) == -1){
        switch(errno)
        {
            case ESRCH:
                return false;
            default:
                std::cerr << "thread-controls.cpp:95:kill(SIGSTOP) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                throw "what?";
        }
    }
    return true;
}

static bool subprocess_continue(pid_t pid){
    if (kill(-pid, SIGCONT) == -1){
        switch(errno)
        {
            case ESRCH:
                return false;
            default:
                std::cerr << "thread-controls.cpp:109:kill(SIGCONT) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                throw "what?";
        }
    }
    return true;
}

static bool write_params_to_subprocess(std::shared_ptr<controller::app::Relation> relation, std::array<int, 2>& pipe, std::string params){
    int len = 0;
    std::size_t bytes_written = 0;
    if(relation->size() == 0){
        // continue to use params if the relation has no dependencies.
        params.append("\n");
    } else {
        // Override params with emplaced parameters.
        boost::json::object jv;
        for (auto& dep: *relation){
            std::string value = dep->acquire_value();
            dep->release_value();
            boost::json::object val = boost::json::parse(value).as_object();
            jv.emplace(dep->key(), val);
        }
        params = boost::json::serialize(jv);
        params.append("\n");
    }
    do{
        len = write(pipe[1], params.data() + bytes_written, params.size() - bytes_written);
        if(len <= 0){
            switch(errno)
            {
                case 0:
                    break;
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:144:write() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else {
           bytes_written += len; 
        }
    }while(bytes_written < params.size());
    return true;
}

static bool wait_for_result_from_subprocess(std::array<int, 2>& pipe, std::unique_ptr<std::atomic<std::size_t> >& state){
    struct pollfd pfd ={
        pipe[0],
        POLLIN,
        0
    };
    int nfds = 0;
    do{
        nfds = poll(&pfd, 1, 5);
        if(nfds < 0){
            switch(errno)
            {
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:273:poll() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else if(nfds == 0){
            return true;
        } else {
            if(pfd.revents & (POLLIN | POLLHUP)){
                state->fetch_add(1, std::memory_order::memory_order_relaxed);
                return true;
            } else {
                return false;
            }
        }
    }while(nfds < 0);
    state->fetch_add(1, std::memory_order::memory_order_relaxed);
    return true;
}

static bool read_result_from_subprocess(std::shared_ptr<controller::app::Relation> relation, std::array<int, 2>& pipe){
    std::array<char, MAX_LENGTH> buf;
    char delimiter = '\n';
    std::string val;
    int len = 0;
    do{
        len = read(pipe[0], buf.data(), MAX_LENGTH);
        if(len < 0){
            switch(errno)
            {
                case EINTR:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:172:read(pipe[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else if(len == 0){
            break;
        } else {
            val.append(buf.data(), len);
        }
    }while(buf.back() != delimiter);
    if(val.empty()){
        relation->acquire_value() = "{}";
        relation->release_value();
    } else {
        relation->acquire_value() = val;
        relation->release_value();
    }
    return true;
}

static void kill_subprocesses(pid_t pid){
    if(kill(-pid, SIGTERM) == -1){
        switch(errno)
        {
            case ESRCH:
                break;
            default:
                std::cerr << "thread-controls.cpp:194:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                throw "what?";
        }
    } else {
        if(kill(-pid, SIGCONT) == -1){
            switch(errno)
            {
                case ESRCH:
                    break;
                default:
                    std::cerr << "thread-controls.cpp:204:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        }
    }
    return;
}

static void close_pipe(std::array<int, 2>& pipe){
    if(close(pipe[0]) == -1){
        std::cerr << "thread-controls.cpp:213:close(pipe[0]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    if(close(pipe[1]) == -1){
        std::cerr << "thread-controls.cpp:217:close(pipe[1]) failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        throw "what?";
    }
    return;
}

namespace controller{
namespace app{
    /* Class Static Members */
    std::chrono::time_point<std::chrono::steady_clock> ThreadControls::start_timer_;
    std::mutex ThreadControls::sched_mtx_;
    std::deque<std::shared_ptr<ThreadSchedHandle> > ThreadControls::sched_handles_;
    void ThreadControls::set_start_time()
    {
        std::unique_lock<std::mutex> lk(ThreadControls::sched_mtx_);
        ThreadControls::start_timer_ = std::chrono::steady_clock::now();
    }

    std::chrono::time_point<std::chrono::steady_clock> ThreadControls::get_start_time()
    {
        std::unique_lock<std::mutex> lk(ThreadControls::sched_mtx_);
        return ThreadControls::start_timer_;
    }

    std::chrono::milliseconds ThreadControls::thread_sched_time_slice()
    {
        std::unique_lock<std::mutex> lk (ThreadControls::sched_mtx_);
        if(ThreadControls::sched_handles_.empty()){
            std::cerr << "thread-controls.cpp:367:Timeslices shouldn't be requested when there are no thread handles to schedule." << std::endl;
            throw "what?";
        } else {
            return ThreadControls::THREAD_SCHED_TIME_SLICE_MS/ThreadControls::sched_handles_.size();
        }
    }

    std::shared_ptr<ThreadSchedHandle> ThreadControls::thread_sched_push()
    {
        std::unique_lock<std::mutex> lk(ThreadControls::sched_mtx_);
        std::shared_ptr<ThreadSchedHandle> handle = std::make_shared<ThreadSchedHandle>();
        sched_handles_.push_front(handle);
        return handle;
    }

    void ThreadControls::thread_sched_yield()
    {
        std::unique_lock<std::mutex> lk(ThreadControls::sched_mtx_);
        if(!ThreadControls::sched_handles_.empty()){
            auto handle = ThreadControls::sched_handles_.front();
            ThreadControls::sched_handles_.pop_front();
            bool finished = handle->finished.load(std::memory_order::memory_order_relaxed);
            if(finished){
                lk.unlock();
                ThreadControls::set_start_time();
                handle->cv.notify_one();
            } else {
                sched_handles_.push_back(handle);
                lk.unlock();
                std::unique_lock<std::mutex> handle_lock(handle->mtx);
                ThreadControls::set_start_time();
                handle->cv.notify_one();
                handle->cv.wait(handle_lock);
                handle_lock.unlock();
            }
        } else {
            lk.unlock();
        }
        return;
    }
    // End of Static Members


    // Thread Controls
    void ThreadControls::wait(){
        std::unique_lock<std::mutex> lk(*mtx_);
        cv_->wait(lk, [&]{ return (signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_START_EVENT); });
        lk.unlock();
        return;
    }

    void ThreadControls::notify(std::size_t idx){
        ctx_mtx_->lock();
        execution_context_idxs_.push_back(idx);
        ctx_mtx_->unlock();
        signal_->fetch_or(CTL_IO_SCHED_START_EVENT, std::memory_order::memory_order_relaxed);
        ThreadControls::thread_sched_yield();
        cv_->notify_one();
        return;
    }  

    std::vector<std::size_t> ThreadControls::stop_thread() {
        ctx_mtx_->lock();
        std::vector<std::size_t> tmp(execution_context_idxs_.begin(), execution_context_idxs_.end());
        execution_context_idxs_.clear();
        ctx_mtx_->unlock();
        if(!is_stopped()){
            // we must guarantee that the thread is unblocked (started) before it can be preempted.
            signal_->fetch_or(CTL_IO_SCHED_START_EVENT | CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
            cv_->notify_one();
        }
        return tmp;
    }

    void ThreadControls::cleanup(){
        if(state_ > 0){
            kill_subprocesses(pid_);
            close_pipe(pipe_);
        }
    }

    bool ThreadControls::thread_continue(){
        switch(state_->load(std::memory_order::memory_order_relaxed))
        {
            case 0:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return fork_exec(pipe_, pid_, relation, env);
            case 1:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return wait_for_launcher(pipe_);
            case 2:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return subprocess_pause(pid_);
            case 3:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return subprocess_continue(pid_);
            case 4:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return write_params_to_subprocess(relation, pipe_, boost::json::serialize(params));
            case 5:
                return wait_for_result_from_subprocess(pipe_, state_);
            case 6:
                state_->fetch_add(1, std::memory_order::memory_order_relaxed);
                return read_result_from_subprocess(relation, pipe_);
            default:
                close_pipe(pipe_);
                return false;
        }
    }
}//namespace app
}//namespace controller