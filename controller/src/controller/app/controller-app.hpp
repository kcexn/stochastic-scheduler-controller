#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include "../../echo-app/utils/common.hpp"
#include "../../utils/uuid.hpp"
#include "../../http-server/http-server.hpp"
#include <boost/context/fiber.hpp>

namespace controller{
namespace app{
    class ExecutionContext
    {
    public:
        ExecutionContext() : execution_context_id_(UUID::uuid_create_v4()) {}
        void resume() { f_ = std::move(f_).resume(); }
        void stop_thread() { stopped_ = true; pthread_exit(0);}
        bool is_stopped() { return stopped_; }
        std::int64_t& start_time() { return start_time_; }
        std::int64_t& end_time() { return end_time_; }
        pthread_t& tid() noexcept { return tid_; }
        std::vector<char>& payload() { return payload_; }
        Http::Request& req() { return req_; }
        Http::Response& res() { return res_; }
        explicit operator bool() const noexcept { return bool(f_); }
        boost::context::fiber& fiber() { return f_; }
        const UUID::uuid_t& execution_context_id() const noexcept { return execution_context_id_; }
    private:
        Http::Request req_;
        Http::Response res_;
        UUID::uuid_t execution_context_id_;
        boost::context::fiber f_;
        bool stopped_;
        std::int64_t start_time_;
        std::int64_t end_time_;
        pthread_t tid_;
        std::vector<char> payload_;
    };

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs);

    class Controller
    {
    public:
        Controller(std::shared_ptr<echo::MailBox> mbox_ptr);
        void start();
        void route_request(Http::Request& req);
        Http::Response create_response(ExecutionContext& ctx);
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
    };

}// namespace app
}// namespace controller

#endif