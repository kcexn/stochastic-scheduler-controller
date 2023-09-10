#include "thread-controls.hpp"
#include <csignal>

#ifdef DEBUG
#include <iostream>
#endif

namespace controller{
namespace app{    
    // Thread Controls
    void ThreadControls::wait(){
         std::unique_lock<std::mutex> lk(*mtx_); 
         cv_->wait(lk, [&](){ 
            return ((signal_->load(std::memory_order::memory_order_relaxed)&echo::Signals::SCHED_START)==echo::Signals::SCHED_START);
        }); 
        return;
    }

    void ThreadControls::notify(std::size_t idx){ 
        mtx_->lock();
        execution_context_idxs_.push_back(idx);
        mtx_->unlock();
        signal_->fetch_or(echo::Signals::SCHED_START, std::memory_order::memory_order_relaxed); 
        cv_->notify_all(); 
        return; 
    }

    std::vector<std::size_t> ThreadControls::invalidate() {
        if((signal_->load(std::memory_order::memory_order_relaxed)&echo::Signals::SCHED_END) == 0){
            pthread_cancel(tid_);
            signal_->fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
            if(pid_ > 0){
                kill(pid_, SIGTERM);
            }
        }
        valid_->store(false, std::memory_order::memory_order_relaxed);  
        mtx_->lock();
        std::vector<std::size_t> tmp(execution_context_idxs_.size());
        std::memcpy(tmp.data(), execution_context_idxs_.data(), execution_context_idxs_.size());
        mtx_->unlock();
        return tmp;
    }

    void ThreadControls::resume() {
        mtx_->lock();
        f_ = std::move(f_).resume();
        mtx_->unlock();
        return;
    }

    void ThreadControls::invalidate_fiber() {
        mtx_->lock();
        f_ = boost::context::fiber();
        mtx_->unlock();
        return;
    }
}//namespace app
}//namespace controller