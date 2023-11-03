#ifndef CONTROLLER_APP_HPP
#define CONTROLLER_APP_HPP
#include <memory>
#include <boost/json.hpp>
#include <application-servers/http/http-server.hpp>
#include "../io/controller-io.hpp"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>


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

namespace libcurl{
    class CurlMultiHandle
    {
    public:
        explicit CurlMultiHandle();
        CURLM* get();
        CURLMcode add_handle(CURL* easy_handle);
        CURLMcode perform(int* nrhp);
        CURLMcode poll(struct curl_waitfd* efds, unsigned int n_efds, int timeout_ms, int* numfds);
        CURLMsg* info_read(int* msgq_len);
        CURLMcode remove_handle(CURL* easy_handle);
        ~CurlMultiHandle();
        struct curl_slist* slist;
        FILE* write_stream;
    private:
        std::vector<CURL*> easy_handles_;
        std::mutex mtx_;
        std::size_t polling_threads_;
        CURLM* mhnd_;
    };
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
        http::HttpResponse create_response(ExecutionContext& ctx);
        void flush_wsk_logs() { 
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
        std::shared_ptr<libcurl::CurlMultiHandle> curl_mhnd_ptr_;
        std::shared_ptr<controller::io::MessageBox> io_mbox_ptr_;
        controller::io::IO io_;
        boost::asio::io_context& ioc_;
    };

}// namespace app
}// namespace controller
#endif