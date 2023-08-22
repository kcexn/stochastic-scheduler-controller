#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include "../../echo-app/utils/common.hpp"
#include "../../sctp-server/sctp.hpp"
#include "../../utils/uuid.hpp"
#include "../../http-server/http-server.hpp"
#include <boost/context/fiber.hpp>

namespace controller{
namespace app{
    class ExecutionContext
    {
    public:
        ExecutionContext(boost::context::fiber&& f)
          : execution_context_id_{UUID::uuid_create_v4()},
            f_{std::move(f)}
        {}

        ExecutionContext(boost::context::fiber&&f, UUID::uuid_t& uuid)
          : execution_context_id_{uuid},
            f_{std::move(f)}
        {}

        void resume() { f_ = std::move(f_).resume(); }
        explicit operator bool() const noexcept { return bool(f_); }
        sctp::assoc_t& assoc_id() noexcept { return assoc_id_; }
        sctp::sid_t& sid() noexcept { return sid_; }
        const UUID::uuid_t execution_context_id() const noexcept { return execution_context_id_; }
    private:
        sctp::assoc_t assoc_id_;
        sctp::sid_t sid_;
        UUID::uuid_t execution_context_id_;
        boost::context::fiber f_;
    };

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs);

    class Controller
    {
    public:
        Controller(std::shared_ptr<echo::MailBox> mbox_ptr);
        void start();
        void route_request(Http::Request req);
        void stop();
        ~Controller();
    private:
        Http::Server server_;
        std::shared_ptr<echo::MailBox> controller_mbox_ptr_;
        std::vector< std::shared_ptr<ExecutionContext> > ctx_ptrs;
        pthread_t tid_;
    };

}// namespace app
}// namespace controller

#endif