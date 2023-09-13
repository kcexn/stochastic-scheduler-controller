#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include "execution-context.hpp"
#include <application-servers/http/http-server.hpp>
#include <application-servers/http/http-session.hpp>
#include "../io/controller-io.hpp"

namespace controller{
namespace app{
    class Controller
    {
    public:
        Controller(std::shared_ptr<echo::MailBox> mbox_ptr, boost::asio::io_context& ioc);
        void start();
        void start_controller();
        void route_request(std::shared_ptr<http::HttpSession>& session);
        http::HttpResponse create_response(ExecutionContext& ctx, boost::json::value& val);
        void flush_wsk_logs() { std::cout << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; std::cerr << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; return;}
        void stop();
        ~Controller();
    private:
        http::HttpServer hs_;
        // Controller Thread ID.
        pthread_t tid_;
        // Global Signals.
        std::shared_ptr<echo::MailBox> controller_mbox_ptr_;
        // Execution Context IDs.
        std::vector<std::shared_ptr<ExecutionContext> > ctx_ptrs;
        // OpenWhisk Action Proxy Initialized.
        bool initialized_;
        // IO
        std::shared_ptr<echo::MailBox> io_mbox_ptr_;
        controller::io::IO io_;
    };
}// namespace app
}// namespace controller

#endif