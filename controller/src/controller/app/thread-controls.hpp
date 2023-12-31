#ifndef THREAD_CONTROLS_HPP
#define THREAD_CONTROLS_HPP
#include <thread>
#include <cstring>
#include <deque>
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <boost/json.hpp>
#include <condition_variable>
#include "../controller-events.hpp"
#include "action-relation.hpp"

namespace controller{
namespace app{

    struct ThreadSchedHandle
    {
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic<bool> flag;
    };

    class ThreadControls
    {
    public:
        static constexpr std::chrono::milliseconds THREAD_SCHED_TIME_SLICE_MS = std::chrono::milliseconds(5);
        static std::chrono::time_point<std::chrono::steady_clock> start_timer_;
        static std::mutex sched_mtx_;
        static std::deque<std::shared_ptr<ThreadSchedHandle> > sched_handles_;
        static void set_start_time();
        static std::chrono::time_point<std::chrono::steady_clock> get_start_time();
        static std::chrono::milliseconds thread_sched_time_slice();
        static std::shared_ptr<ThreadSchedHandle> thread_sched_push();
        static void thread_sched_yield(bool finished);

        explicit ThreadControls():
            pid_{0},
            mtx_(std::make_unique<std::mutex>()), 
            ctx_mtx_(std::make_unique<std::mutex>()),
            cv_(std::make_unique<std::condition_variable>()), 
            signal_(std::make_unique<std::atomic<std::uint16_t> >()),
            ctx_cv_(std::make_unique<std::condition_variable>()),
            state_(std::make_unique<std::atomic<std::size_t> >(0)),
            pipe_{}
        {}
        boost::json::object params;
        std::map<std::string, std::string> env;
        std::shared_ptr<Relation> relation;
        std::atomic<std::uint16_t>& signal() { return *signal_; }
        void wait();
        void notify(std::size_t idx);
        bool is_started() const { return ((signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_START_EVENT) != 0);}
        bool is_stopped() const { return ((signal_->load(std::memory_order::memory_order_relaxed) & CTL_IO_SCHED_END_EVENT) != 0); }
        std::size_t state() const { return state_->load(std::memory_order::memory_order_relaxed); }
        bool has_pending_idxs() const { ctx_mtx_->lock(); std::size_t len = execution_context_idxs_.size(); ctx_mtx_->unlock(); return (len > 0);}
        std::vector<std::size_t> pop_idxs() { ctx_mtx_->lock(); std::vector<std::size_t> tmp(execution_context_idxs_.begin(), execution_context_idxs_.end()); execution_context_idxs_.clear(); ctx_mtx_->unlock(); return tmp; }
        std::vector<std::size_t> stop_thread();
        void acquire(){ ctx_mtx_->lock(); return; }
        void release(){ ctx_mtx_->unlock(); return; }
        pid_t& pid() { return pid_; }
        void cleanup();
        bool thread_continue();
    private:
        pid_t pid_;
        std::unique_ptr<std::mutex> mtx_;
        std::unique_ptr<std::mutex> ctx_mtx_;
        std::unique_ptr<std::condition_variable> cv_;
        std::unique_ptr<std::atomic<std::uint16_t> > signal_;
        std::unique_ptr<std::condition_variable> ctx_cv_;
        std::unique_ptr<std::atomic<std::size_t> > state_;
        std::array<int, 2> pipe_;
        std::vector<std::size_t> execution_context_idxs_;
    };
}//namespace app
}//namespace controller
#endif