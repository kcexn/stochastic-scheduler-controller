#ifndef EXECUTION_CONTEXT_HPP
#define EXECUTION_CONTEXT_HPP
#include <boost/context/fiber.hpp>
#include "../../http-server/http-server.hpp"
#include "action-manifest.hpp"
#include "thread-controls.hpp"

namespace controller{
namespace app{
class ExecutionContext
    {
    public:
        constexpr static struct Init{} init{};
        constexpr static struct Run{} run{};
        
        ExecutionContext(): execution_context_id_(UUID::Uuid(UUID::Uuid::v4)), execution_context_idx_stack_{0} {}
        explicit ExecutionContext(Init init);
        explicit ExecutionContext(Run run);
        explicit ExecutionContext(Run run, const UUID::Uuid& uuid);
        bool is_stopped();
        std::vector<std::shared_ptr<Http::Request> >& reqs() { return requests_; }
        Http::Response& res() { return res_; }
        const UUID::Uuid& execution_context_id() const noexcept { return execution_context_id_; }
        ActionManifest& manifest() { return manifest_; }

        // Action Manifest members.
        std::size_t pop_execution_idx();
        void push_execution_idx(std::size_t idx);

        // Thread Control Members
        std::vector<ThreadControls>& thread_controls() { return thread_controls_; }
    private:
        std::vector<std::shared_ptr<Http::Request> > requests_;
        Http::Response res_;
        UUID::Uuid execution_context_id_;

        // Action Manifest variables
        ActionManifest manifest_;
        std::vector<std::size_t> execution_context_idx_stack_;

        // Thread Control Data Elements
        std::vector<ThreadControls> thread_controls_;
    };
    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs);
}//namepsace app
}//namespace controller
#endif