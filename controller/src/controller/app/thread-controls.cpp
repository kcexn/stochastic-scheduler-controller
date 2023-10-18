#include "thread-controls.hpp"
#include <csignal>

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
        cv_->notify_all(); 
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
        if(!is_stopped()){
            if(pid_ > 0){
                kill(-pid_, SIGKILL);
            }
            ctx_mtx_->lock();
            pthread_cancel(tid_);
            ctx_mtx_->unlock();
            // Stop the thread.
            signal_->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
        }
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

    void ThreadControls::resume() {
        ctx_mtx_->lock();
        f_ = std::move(f_).resume_with([&](boost::context::fiber&& f2){
            ctx_mtx_->unlock();
            return std::move(f2);
        });
        return;
    }

    void ThreadControls::invalidate_fiber() {
        f_ = boost::context::fiber();
        return;
    }
}//namespace app
}//namespace controller