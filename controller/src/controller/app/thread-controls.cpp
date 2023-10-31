#include "thread-controls.hpp"
#include <csignal>
#include <iostream>
#include <sys/resource.h>

namespace controller{
namespace app{    
    // Thread Controls
    void ThreadControls::wait(){
        std::unique_lock<std::mutex> lk2(*mtx_);
        cv_->wait(lk2, [&]{ return (signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_START_EVENT); });
        lk2.unlock();
        return;
    }

    void ThreadControls::notify(std::size_t idx){
        signal_->fetch_or(CTL_IO_SCHED_START_EVENT, std::memory_order::memory_order_relaxed);
        cv_->notify_all();
        std::unique_lock<std::mutex> lk1(*ctx_mtx_);
        execution_context_idxs_.push_back(idx);
        lk1.unlock();
        return;
    }

    void ThreadControls::kill_subprocesses(){
        if(kill(-pid_, SIGTERM) == -1){
            switch(errno)
            {
                case ESRCH:
                    break;
                default:
                    std::cerr << "thread-controls.hpp:50:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                    throw "what?";
            }
        } else {
            if(kill(-pid_, SIGCONT) == -1){
                switch(errno)
                {
                    case ESRCH:
                        break;
                    default:
                        std::cerr << "thread-controls.hpp:70:kill() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "what?";
                }
            }
        }
    }

    std::vector<std::size_t> ThreadControls::stop_thread() {
        std::vector<std::size_t> tmp;
        std::unique_lock<std::mutex> lk2(*mtx_);
        cv_->wait(lk2, [&](){ return (ready_->load(std::memory_order::memory_order_relaxed)); });
        lk2.unlock();
        if(!is_stopped()){
            signal_->fetch_or(CTL_IO_SCHED_START_EVENT | CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
            cv_->notify_all();
            if(pid_ > 0){

            }
        }
        std::unique_lock<std::mutex> lk1(*ctx_mtx_);
        if(!execution_context_idxs_.empty()){
            tmp.insert(tmp.end(), execution_context_idxs_.begin(), execution_context_idxs_.end());
            // Clear the execution context idx vector so that subsequent invalidates do not
            // receive a list of executions indexes to assign work to.
            execution_context_idxs_.clear();
        }
        lk1.unlock();
        return tmp;
    }

    void ThreadControls::resume() {
        f_ = std::move(f_).resume();
        return;
    }

    void ThreadControls::invalidate_fiber() {
        f_ = boost::context::fiber();
        return;
    }
}//namespace app
}//namespace controller