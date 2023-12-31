#ifndef EXECUTION_CONTEXT_HPP
#define EXECUTION_CONTEXT_HPP
#include <vector>
#include <memory>
#include <uuid/uuid.hpp>
#include "action-manifest.hpp"
#include "thread-controls.hpp"
#include <transport-servers/server/server.hpp>
#include <map>

#ifdef OW_PROFILE
#include <chrono>
#endif

namespace http{
    class HttpClientSession;
}

namespace controller{
namespace resources{
    enum class Routes 
    {
        RUN,
        INIT
    };
}// resources
}// controller

namespace controller{
namespace app{
    class ExecutionContext
    {
    public:
        constexpr static struct Init{} init{};
        constexpr static struct Run{} run{};
       
        #ifdef OW_PROFILE
        // Profiling timers.
        std::chrono::time_point<std::chrono::steady_clock> start_;
        #endif

        ExecutionContext(): execution_context_id_(UUID::Uuid(UUID::Uuid::v4)), execution_context_idx_stack_{0} {}
        explicit ExecutionContext(Init init);
        explicit ExecutionContext(Run run, const std::map<std::string, std::string>& env);
        explicit ExecutionContext(Run run, const UUID::Uuid& uuid, std::size_t idx, const std::vector<std::string>& peers, const std::map<std::string, std::string>& env);
        bool is_stopped();

        std::vector<std::shared_ptr<http::HttpSession> >& sessions() { return http_session_ptrs_; }
        std::vector<std::shared_ptr<http::HttpClientSession> >& ow_client_sessions() { return http_ow_client_sessions_; }
        std::vector<std::shared_ptr<http::HttpSession> >& peer_server_sessions() { return http_peer_server_sessions_; }
        std::vector<std::shared_ptr<http::HttpClientSession> >& peer_client_sessions() { return http_peer_client_sessions_; }
        std::vector<server::Remote>& peer_addresses() { return peers_; }
        std::vector<server::Remote> get_peers() { mtx_.lock(); std::vector<server::Remote> tmp = peers_; mtx_.unlock(); return tmp;}
        void merge_peer_addresses(const std::vector<std::string>&);

        const UUID::Uuid& execution_context_id() const { return execution_context_id_; }
        ActionManifest& manifest() { return manifest_; }
        std::vector<std::size_t>& execution_context_idx_array() { return execution_context_idx_array_; }
        const controller::resources::Routes& route() { return route_; }

        // Action Manifest members.
        std::size_t pop_execution_idx();
        void push_execution_idx(std::size_t idx);

        // Thread Control Members
        std::vector<ThreadControls>& thread_controls() { return thread_controls_; }
        void acquire(){sync_.lock(); return;}
        void release(){sync_.unlock(); return;}

        std::map<std::string, std::string>& env(){ return env_; }

        ~ExecutionContext(); 

    private:
        /* ow invoker server sessions are kept as http_session_ptrs_ for backwards compatibility. */
        std::vector<std::shared_ptr<http::HttpSession> > http_session_ptrs_;
        // Http Session associated to the execution context.
        std::vector<std::shared_ptr<http::HttpClientSession> > http_ow_client_sessions_;
        std::vector<std::shared_ptr<http::HttpSession> > http_peer_server_sessions_;
        std::vector<std::shared_ptr<http::HttpClientSession> > http_peer_client_sessions_;

        UUID::Uuid execution_context_id_;
        // Action Manifest variables
        ActionManifest manifest_;
        std::vector<std::size_t> execution_context_idx_stack_;
        std::vector<std::size_t> execution_context_idx_array_;
        controller::resources::Routes route_;

        // Thread Control Data Elements
        std::vector<ThreadControls> thread_controls_;

        // Execution Context Peering Members.
        std::vector<server::Remote> peers_;
        std::mutex mtx_;

        // Synchronization
        std::mutex sync_;
        std::atomic<std::size_t> sync_counter_;
        std::condition_variable sync_cv_;

        // Environment variables
        std::map<std::string, std::string> env_;
    };
    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs);
}//namepsace app
}//namespace controller
#endif