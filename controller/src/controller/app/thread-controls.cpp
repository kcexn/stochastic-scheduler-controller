#include "thread-controls.hpp"
#include <csignal>
#include <iostream>
#include <sys/resource.h>

namespace controller{
namespace app{    
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
        cv_->notify_one(); 
        return; 
    }

    std::vector<std::size_t> ThreadControls::invalidate() {
        valid_->store(false, std::memory_order::memory_order_relaxed); 
        std::vector<std::size_t> tmp;
        ctx_mtx_->lock();
        if(!execution_context_idxs_.empty()){
            tmp.insert(tmp.end(), execution_context_idxs_.begin(), execution_context_idxs_.end());
            // Clear the execution context idx vector so that subsequent invalidates do not
            // receive a list of executions indexes to assign work to.
            execution_context_idxs_.clear();
        }
        ctx_mtx_->unlock();
        return tmp;
    }


    std::vector<std::size_t> ThreadControls::stop_thread() {
        std::vector<std::size_t> tmp;
        if(!is_stopped()){
            if(pid_ > 0){
                if(kill(-pid_, SIGTERM) == -1){
                    switch(errno)
                    {
                        case ESRCH:
                            break;
                        default:
                            std::cerr << "thread-controls.cpp:50:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            throw "what?";
                    }
                } else {
                    if(kill(-pid_, SIGCONT) == -1){
                        switch(errno)
                        {
                            case ESRCH:
                                break;
                            default:
                                std::cerr << "thread-controls.cpp:70:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "what?";
                        }
                    }
                }
            }
            int status = pthread_cancel(tid_);
            switch(status)
            {
                case 0:
                    // all good.
                    break;
                default:
                {
                    int errsv = status;
                    errno = 0;
                    struct timespec ts = {};
                    status = clock_gettime(CLOCK_REALTIME, &ts);
                    if(status == -1){
                        std::cerr << "thread-controls.cpp:92:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        std::cerr << "thread-controls.cpp:93:pthread_cancel failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                    } else {
                        std::cerr << "thread-controls.cpp:95:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":pthread_cancel failed:" << std::make_error_code(std::errc(errsv)).message() << std::endl;
                    }
                }
            }
        }
        signal_->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
        ctx_mtx_->lock();
        if(!execution_context_idxs_.empty()){
            tmp.insert(tmp.end(), execution_context_idxs_.begin(), execution_context_idxs_.end());
            // Clear the execution context idx vector so that subsequent invalidates do not
            // receive a list of executions indexes to assign work to.
            execution_context_idxs_.clear();
        }
        ctx_mtx_->unlock();
        return tmp;
    }

    void ThreadControls::resume() {
        if(f_){
            f_ = std::move(f_).resume();
        } else {
            struct timespec ts = {};
            int status = clock_gettime(CLOCK_REALTIME, &ts);
            if(status == -1){
                std::cerr << "thread-controls.cpp:119:clock_gettime failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                std::cerr << "thread-controls.cpp:120:thread resume called when the thread context is not valid" << std::endl;
            } else {
                std::cerr << "thread-controls.cpp:122:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":thread resume called when the thread context is not valid." << std::endl;
            }
        }
        return;
    }

    void ThreadControls::invalidate_fiber() {
        f_ = boost::context::fiber();
        return;
    }
}//namespace app
}//namespace controller