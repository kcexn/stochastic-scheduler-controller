#include "thread-controls.hpp"
#include <csignal>

namespace controller{
namespace app{    
    // Thread Controls
    void ThreadControls::wait(){
        std::unique_lock<std::mutex> lk(*mtx_); 
        cv_->wait(lk, [&]{ return (signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_START_EVENT); }); 
        return;
    }

    void ThreadControls::notify(std::size_t idx){ 
        execution_context_idxs_.push_back(idx);
        signal_->fetch_or(CTL_IO_SCHED_START_EVENT, std::memory_order::memory_order_relaxed); 
        cv_->notify_all(); 
        return; 
    }

    std::vector<std::size_t> ThreadControls::invalidate() {
        valid_->store(false, std::memory_order::memory_order_relaxed);  
        std::vector<std::size_t> tmp(execution_context_idxs_.size());
        std::memcpy(tmp.data(), execution_context_idxs_.data(), execution_context_idxs_.size());
        // Clear the execution context idx vector so that subsequent invalidates do not
        // receive a list of executions indexes to assign work to.
        execution_context_idxs_.clear();
        return tmp;
    }


    std::vector<std::size_t> ThreadControls::stop_thread() {
        if(!is_stopped()){
            pthread_cancel(tid_);
            // Stop the thread.
            signal_->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
            if(pid_ > 0){
                kill(pid_, SIGTERM);
            }
        }
        std::vector<std::size_t> tmp;
        tmp.reserve(execution_context_idxs_.size());
        if(!execution_context_idxs_.empty()){
            tmp.insert(tmp.end(), execution_context_idxs_.begin(), execution_context_idxs_.end());
            // Clear the execution context idx vector so that subsequent invalidates do not
            // receive a list of executions indexes to assign work to.
            execution_context_idxs_.clear();
        }
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