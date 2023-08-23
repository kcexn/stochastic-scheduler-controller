#include "controller-app.hpp"
#include "../resources/run/run.hpp"
#include <boost/context/fiber.hpp>
#include <thread>
#include <boost/json.hpp>
#include <functional>
#include <ctime>

#ifdef DEBUG
#include <iostream>
#endif
namespace controller{
namespace app{
// Execution Context
    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs){
        return lhs.execution_context_id() == rhs.execution_context_id();
    }

// Controller Class
    Controller::Controller(std::shared_ptr<echo::MailBox> mbox_ptr)
      : controller_mbox_ptr_(mbox_ptr)
    {
        #ifdef DEBUG
        std::cout << "Application Controller Constructor!" << std::endl;
        #endif
        std::thread controller(
            &Controller::start, this
        );
        tid_ = controller.native_handle();
        controller.detach();
    }

    void Controller::start(){
        // Initialize resources I might need.
        std::unique_lock<std::mutex> lk(controller_mbox_ptr_->mbx_mtx, std::defer_lock);
        #ifdef DEBUG
        std::cout << "HTTP Server Started!" << std::endl;
        #endif

        // Scheduling Loop.
        while(true){
            lk.lock();
            controller_mbox_ptr_->mbx_cv.wait(lk, [&]{ return (controller_mbox_ptr_->msg_flag.load() == true || controller_mbox_ptr_->signal.load() != 0); });
            lk.unlock();

            if (controller_mbox_ptr_->msg_flag.load()){
                lk.lock();
                Http::Session session( controller_mbox_ptr_->session_ptr );
                lk.unlock();
                controller_mbox_ptr_->msg_flag.store(false);
                if ( (controller_mbox_ptr_->signal.load() & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                    pthread_exit(0);
                }
                std::vector<Http::Session>& sessions = server_.http_sessions();
                auto it = std::find(sessions.begin(), sessions.end(), session);
                Http::Request request = {};
                if (it != sessions.end()){
                    // Do Something.
                    request = it->read_request();
                    #ifdef DEBUG
                    std::cout << "Session is in the HTTP Server." << std::endl;
                    #endif
                } else {
                    sessions.push_back(std::move(session));
                    request = server_.http_sessions().back().read_request();
                    #ifdef DEBUG
                    std::cout << "Session is not in the HTTP Server." << std::endl;
                    #endif
                }
                route_request(request);
                #ifdef DEBUG
                std::cout << "HTTP Server Loop!" << std::endl;
                #endif
            }
            
            if ( (controller_mbox_ptr_->signal.load() & echo::Signals::SCHED_END) == echo::Signals::SCHED_END ){
                controller_mbox_ptr_->signal.fetch_and(~echo::Signals::SCHED_END);
                for( auto ctx_ptr : ctx_ptrs ){
                    if (ctx_ptr->is_stopped()){
                        Http::Response res = create_response(*ctx_ptr);
                        std::stringstream ss;
                        ss << "HTTP/1.1 " << res.status_code << " " << res.status_message << "\r\n"
                           << "Content-Type: application/json\r\n"
                           << "Content-Length: " << res.content_length << "\r\n"
                           << "\r\n"
                           << res.body;
                        #ifdef DEBUG
                        std::cout << ss.str() << std::endl;
                        #endif
                        for ( Http::Session session: sessions()){
                            if ( session.request() == ctx_ptr->req() ){
                                std::string str(ss.str());

                                //TODO: This DEFINITELY shouldn't go here.
                                //Write the http response to the unix socket with a unique fd.
                                session.socket().write_some(boost::asio::buffer(str.data(), str.size()));
                                session.socket().shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
                                session.socket().close();
                                auto it = std::find(sessions().begin(), sessions().end(), session);
                                sessions().erase(it);
                            }
                        }
                    }
                }
            }
        }
    }

    void Controller::route_request(Http::Request req){
        if (req.body_fully_formed){
            try{
                boost::json::error_code ec;
                boost::json::value val = boost::json::parse(req.body, ec);
                if (ec) {
                    #ifdef DEBUG
                    std::cout << "Parsing failed: " << ec.message() << std::endl;
                    #endif
                }
                // Route the request.
                if (req.route == "/run" ){
                    controller::resources::run::Request run(val.get_object());

                    // Create a fiber continuation for processing the request.
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run);                  
                    std::thread executor(
                        [&, ctx_ptr](std::shared_ptr<echo::MailBox> mbox_ptr){
                            ctx_ptr->start_time() = static_cast<std::int64_t>(time(NULL));
                            ctx_ptr->resume();
                            ctx_ptr->end_time() = static_cast<std::int64_t>(time(NULL));
                            mbox_ptr->signal.fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                            mbox_ptr->mbx_cv.notify_all();
                            ctx_ptr->stop_thread();
                        }, controller_mbox_ptr_
                    );
                    ctx_ptr->tid() = executor.native_handle();
                    ctx_ptr->req() = req;
                    ctx_ptrs.push_back(std::move(ctx_ptr));
                    executor.detach();
                    #ifdef DEBUG
                    std::cout << run.deadline() << std::endl;
                    #endif
                } else if (req.route == "/init" ) {
                    // Do Something.
                }
            } catch ( std::bad_alloc const& e){
                #ifdef DEBUG
                std::cout << "Parsing Failed: " << e.what() << std::endl;
                #endif
            }

            // Route the request.
            #ifdef DEBUG
            std::cout << req.verb << std::endl;
            std::cout << req.route << std::endl;
            std::cout << req.content_length << std::endl;
            std::cout << std::boolalpha << req.headers_fully_formed << std::endl;
            std::cout << req.body << std::endl;
            std::cout << std::boolalpha << req.body_fully_formed << std::endl;
            #endif
        }
    }

    Http::Response Controller::create_response(ExecutionContext& ctx){
        if ( ctx.req().route == "/run" ){
            std::string result(ctx.payload().begin(), ctx.payload().end());
            boost::json::value jv = boost::json::parse(result);
            controller::resources::run::Response response = {
                .status = "success",
                .status_code = 0,
                .success = true,
                .result = jv.as_object()
            };

            boost::json::value jv_res = {
                {"status", response.status },
                {"status_code", response.status_code },
                {"success", response.success },
                {"result", response.result }
            };

            boost::json::object req_body = boost::json::parse(ctx.req().body).as_object();
            
            controller::resources::run::ActivationRecord record = {
                .activation_id = req_body["activation_id"].as_string(),
                .name_space = req_body["namespace"].as_string(),
                .action_name = req_body["action_name"].as_string(),
                .start_time = ctx.start_time(),
                .end_time = ctx.end_time(),
                .logs = {"LOG:1", "LOG:2" },
                .annotations = { "ANOOTATION:1", "ANNOTATION:2" },
                .response = jv_res.as_object()
            };

            boost::json::value jv_activation_record = {
                {"activation_id", record.activation_id },
                {"namespace", record.name_space },
                {"name", record.action_name },
                {"start", record.start_time },
                {"end", record.end_time },
                {"logs", record.logs },
                {"annotations", record.annotations },
                {"response", record.response }
            };

            std::stringstream ss;
            ss << jv_activation_record;
            Http::Response res = {
                .status_code = "200",
                .status_message = "OK",
                .location = "http://localhost:8080",
                .content_length = ss.str().size(),
                .body = ss.str(),
            };

            #ifdef DEBUG
            std::cout << "Status Code: " << res.status_code << " : " << "Status Message: " << " : " << res.status_message << " : " << "Location: " << res.location << " : " << "Content-Length: " << res.content_length 
                      << " : " << "Body: " << res.body << std::endl;
            #endif

            return res;
        } else {
            throw;
        }
    }

    std::vector<Http::Session>& Controller::sessions(){
        return server_.http_sessions();
    }

    void Controller::stop(){
        pthread_cancel(tid_);
    }

    Controller::~Controller()
    {
        #ifdef DEBUG
        std::cout << "Application Controller Destructor!" << std::endl;
        #endif
        stop();
    }
}// namespace app
}//namespace controller