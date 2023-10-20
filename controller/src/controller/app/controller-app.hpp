#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include <memory>
#include <boost/json.hpp>
#include <application-servers/http/http-server.hpp>
#include "../io/controller-io.hpp"
#include <iostream>
#include <filesystem>


/*Forward Declarations*/
namespace controller{
namespace app{
    class ExecutionContext;
}
}
namespace http{
    class HttpSession;
    class HttpClientSession;
    class HttpResponse;
}

namespace controller{
namespace app{
    class Controller
    {
    public:
        Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc);
        Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc, const std::filesystem::path& upath, std::uint16_t sport);
        void start();
        void start_controller();
        void route_response(std::shared_ptr<http::HttpClientSession>& session);
        void route_request(std::shared_ptr<http::HttpSession>& session);
        http::HttpResponse create_response(ExecutionContext& ctx, boost::json::value& val);
        void flush_wsk_logs() { 
            // struct timespec ts;
            // int ec = clock_gettime(CLOCK_REALTIME, &ts);
            // if(ec == -1){
            //     std::cerr << "clock_gettime(CLOCK_REALTIME, &ts): " << std::make_error_code(std::errc(errno)).message() << std::endl;
            // } else {
            //     std::cout << "end: " << ((ts.tv_sec*1000) + (ts.tv_nsec/1000000)) << std::endl;
            // }
            std::cout << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; 
            std::cerr << "XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX" << std::endl; 
            return;
        }
        void stop();
        ~Controller();
    private:
        // Http Server.
        http::HttpServer hs_;
        // Http Client Server.
        http::HttpServer hcs_;

        // Controller Thread ID.
        pthread_t tid_;
        // Global Signals.
        std::shared_ptr<controller::io::MessageBox> controller_mbox_ptr_;
        // Execution Context IDs.
        std::vector<std::shared_ptr<ExecutionContext> > ctx_ptrs;
        // OpenWhisk Action Proxy Initialized.
        bool initialized_;
        // IO
        std::shared_ptr<controller::io::MessageBox> io_mbox_ptr_;
        controller::io::IO io_;
        boost::asio::io_context& ioc_;
    };

}// namespace app
}// namespace controller
#endif