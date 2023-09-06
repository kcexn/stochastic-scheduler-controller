#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include "../../echo-app/utils/common.hpp"
#include "../../utils/uuid.hpp"
#include "../../http-server/http-server.hpp"
#include <boost/json.hpp>
#include <boost/context/fiber.hpp>
#include "../io/controller-io.hpp"
#include <filesystem>

namespace controller{
namespace app{
    class Relation
    {
    public:
        Relation(const std::string& key, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies);
        Relation( std::string&& key, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies);

        Relation(const std::string& key, const std::string& value, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies);
        Relation(std::string&& key, std::string&& value, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies);

        const std::string& key() const { return kvp_.first; }
        std::string& value() { return kvp_.second; }
        const std::size_t& depth() const { return depth_; }
        const std::filesystem::path& path() const { return path_; }

        // Reexport the std::vector interface.
        std::vector<std::shared_ptr<Relation> >::iterator begin() { return dependencies_.begin(); }
        std::vector<std::shared_ptr<Relation> >::iterator end() { return dependencies_.end(); }
        std::vector<std::shared_ptr<Relation> >::const_iterator cbegin() { return dependencies_.cbegin(); }
        std::vector<std::shared_ptr<Relation> >::const_iterator cend() { return dependencies_.cend(); }
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rbegin() { return dependencies_.rbegin(); }
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rend() { return dependencies_.rend(); }
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crbegin() { return dependencies_.crbegin(); }
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crend() { return dependencies_.crend(); }

        std::shared_ptr<Relation>& operator[]( std::vector<std::shared_ptr<Relation> >::size_type pos ){ return dependencies_[pos]; }
        std::size_t size(){ return dependencies_.size(); }
    private:
        std::pair<std::string, std::string> kvp_;
        std::vector<std::shared_ptr<Relation> > dependencies_;
        std::size_t depth_;
        std::filesystem::path path_;
    };

    class ActionManifest
    {
    public:
        ActionManifest();
        void emplace(const std::string& key, const boost::json::object& manifest);
        std::shared_ptr<Relation> next(const std::string& key, const std::size_t& idx);

        // Reexport the std::vector interface.
        std::vector<std::shared_ptr<Relation> >::iterator begin();
        std::vector<std::shared_ptr<Relation> >::iterator end();
        std::vector<std::shared_ptr<Relation> >::const_iterator cbegin();
        std::vector<std::shared_ptr<Relation> >::const_iterator cend();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rbegin();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rend();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crbegin();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crend();

        std::shared_ptr<Relation>& operator[](std::vector<std::shared_ptr<Relation> >::size_type pos);

        void push_back(const std::shared_ptr<Relation>& relation) { index_.push_back(relation); return; }
        void push_back(std::shared_ptr<Relation>&& relation) { index_.push_back(relation); return; }

        std::vector<std::shared_ptr<Relation> >::size_type size(){ return index_.size(); }
    private:
        std::vector<std::shared_ptr<Relation> > index_;
    };

    class ThreadControls
    {
    public:
        explicit ThreadControls(): 
            mtx_(std::make_unique<std::mutex>()), 
            cv_(std::make_unique<std::condition_variable>()), 
            signal_(std::make_unique<std::atomic<int> >()),
            valid_(std::make_unique<std::atomic<bool> >(true)),
            execution_context_idx_(std::make_unique<std::atomic<std::size_t> >(0))
        {}
        pthread_t& tid() { return tid_; }
        std::atomic<int>& signal() { return *signal_; }
        void wait();
        void notify(std::size_t idx);
        const bool is_stopped() const { return ((signal_->load(std::memory_order::memory_order_relaxed) & echo::Signals::SCHED_END) == echo::Signals::SCHED_END); }
        const bool is_valid() const { return valid_->load(std::memory_order::memory_order_relaxed); }
        std::size_t invalidate() { valid_->store(false, std::memory_order::memory_order_relaxed); return execution_context_idx_->load(std::memory_order::memory_order_relaxed); }
    private:
        pthread_t tid_;
        std::unique_ptr<std::mutex> mtx_;
        std::unique_ptr<std::condition_variable> cv_;
        std::unique_ptr<std::atomic<int> > signal_;
        std::unique_ptr<std::atomic<bool> > valid_;
        std::unique_ptr<std::atomic<std::size_t> > execution_context_idx_;
    };

    class ExecutionContext
    {
    public:
        struct Init{};
        struct Run{};
        
        ExecutionContext(): execution_context_id_(UUID::uuid_create_v4()) {}
        explicit ExecutionContext(Init init);
        explicit ExecutionContext(Run run);
        // explicit ExecutionContext(Run run, UUID::uuid_t execution_context_id, std::size_t execution_context_idx);
        bool is_stopped();
        Http::Request& req() { return req_; }
        Http::Response& res() { return res_; }
        const UUID::uuid_t& execution_context_id() const noexcept { return execution_context_id_; }
        ActionManifest& manifest() { return manifest_; }

        // Context data elements.
        std::size_t& idx() { return execution_context_index_; }
        std::vector<boost::context::fiber>& acquire_fibers() { fiber_mtx_.lock(); return fibers_; }
        void release_fibers() { fiber_mtx_.unlock(); return; }

        // Thread Control Members
        std::vector<ThreadControls>& thread_controls() { return thread_controls_; }
    private:
        Http::Request req_;
        Http::Response res_;
        UUID::uuid_t execution_context_id_;

        // Action Manifest variables
        ActionManifest manifest_;

        // TODO: this index will be used later to modify the behaviour of the 
        // depth first search for the next incomplete dependency.
        std::size_t execution_context_index_;

        // Context data elements.
        std::vector<boost::context::fiber> fibers_;
        std::mutex fiber_mtx_;

        // Thread Control Data Elements
        std::vector<ThreadControls> thread_controls_;
    };

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs);

    class Controller
    {
    public:
        // Controller(std::shared_ptr<echo::MailBox> mbox_ptr);
        Controller(std::shared_ptr<echo::MailBox> mbox_ptr, boost::asio::io_context& ioc);
        void start();
        void start_controller();
        void route_request(Http::Request& req );
        Http::Response create_response(ExecutionContext& ctx);
        void flush_wsk_logs() { std::cout << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; std::cerr << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; return;}
        void stop();
        ~Controller();
    private:
        Http::Server server_;
        // Controller Thread ID.
        pthread_t tid_;
        // Global Signals.
        std::shared_ptr<echo::MailBox> controller_mbox_ptr_;
        // Execution Context IDs.
        std::vector< std::shared_ptr<ExecutionContext> > ctx_ptrs;
        // OpenWhisk Action Proxy Initialized.
        bool initialized_;
        // IO
        std::shared_ptr<echo::MailBox> io_mbox_ptr_;
        controller::io::IO io_;
    };

}// namespace app
}// namespace controller

#endif