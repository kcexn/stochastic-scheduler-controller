#ifndef THREAD_CONTROLS_HPP
#define THREAD_CONTROLS_HPP
#include <thread>
#include <cstring>
#include <atomic>
#include <memory>
#include <vector>
#include <boost/context/fiber.hpp>
#include <mutex>
#include <condition_variable>
#include "../controller-events.hpp"

namespace controller{
namespace app{
    class ThreadControls
    {
    public:
        explicit ThreadControls(): 
            pid_{0},
            mtx_(std::make_unique<std::mutex>()), 
            cv_(std::make_unique<std::condition_variable>()), 
            signal_(std::make_unique<std::atomic<std::uint16_t> >()),
            valid_(std::make_unique<std::atomic<bool> >(true))
        {}
        pthread_t& tid() { return tid_; }
        std::atomic<std::uint16_t>& signal() { return *signal_; }
        void wait();
        void notify(std::size_t idx);
        const bool is_stopped() const { return ((signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_END_EVENT) == CTL_IO_SCHED_END_EVENT); }
        const bool is_valid() const { return valid_->load(std::memory_order::memory_order_relaxed); }
        std::vector<std::size_t> invalidate();
        boost::context::fiber& f() { return f_; }
        void resume();
        void invalidate_fiber();
        pid_t& pid() { return pid_; }
    private:
        pthread_t tid_;
        pid_t pid_;
        std::unique_ptr<std::mutex> mtx_;
        std::unique_ptr<std::condition_variable> cv_;
        std::unique_ptr<std::atomic<std::uint16_t> > signal_;
        std::unique_ptr<std::atomic<bool> > valid_;
        std::vector<std::size_t> execution_context_idxs_;
        boost::context::fiber f_;
    };
}//namespace app
}//namespace controller
#endif