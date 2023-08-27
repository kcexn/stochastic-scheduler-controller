#include "controller-app.hpp"
#include "../resources/resources.hpp"
#include <boost/context/fiber.hpp>
#include <thread>
#include <boost/json.hpp>
#include <functional>
#include <ctime>
#include <fstream>

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
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false}
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
                    request = sessions.back().read_request();
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
                auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [](std::shared_ptr<ExecutionContext> ctx_ptr){ return ctx_ptr->is_stopped(); });
                while ( it != ctx_ptrs.end() ){
                    Http::Response res = create_response(**it);
                    std::stringstream ss;
                    ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                        << "Connection: " << res.connection << "\r\n"
                        << "Content-Type: application/json\r\n"
                        << "Content-Length: " << res.content_length << "\r\n"
                        << "\r\n"
                        << res.body;
                    #ifdef DEBUG
                    std::cout << ss.str() << std::endl;
                    #endif
                    auto session_it = std::find_if(server_.http_sessions().begin(), server_.http_sessions().end(), [&](auto session){ return session.request() == (*it)->req(); });
                    if (session_it != server_.http_sessions().end()){
                        std::string str(ss.str());
                        //Write the http response to the unix socket with a unique fd.
                        lk.lock();
                        controller_mbox_ptr_->mbx_cv.wait(lk, [&](){ return ((controller_mbox_ptr_->signal.load() & echo::Signals::APP_UNIX_WRITE) == 0); });
                        controller_mbox_ptr_->payload_buffer_ptr = std::make_shared<std::vector<char> >(str.begin(), str.end());
                        controller_mbox_ptr_->session_ptr = std::shared_ptr<UnixServer::Session>(session_it->unix_session());
                        lk.unlock();
                        controller_mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                        controller_mbox_ptr_->signal.fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                        controller_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                        server_.http_sessions().erase(session_it);
                    } // else the request is no longer in the http sessions table, so we erase the context, and do nothing.
                    ctx_ptrs.erase(it);
                    it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [](std::shared_ptr<ExecutionContext> ctx_ptr){ return ctx_ptr->is_stopped(); });
                }
            }
        }
    }

    void Controller::route_request(Http::Request& req){
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
                            // The first resume sets up the action runtime environment for execution.
                            // The action runtime doesn't have to be set up in a distinct 
                            // thread of execution, but since we need to take the time to set up 
                            // a thread anyway, deferring the process fork in the execution context until after the 
                            // thread is established so that the fork can happen concurrently 
                            // is a more performant solution.
                            ctx_ptr->resume();

                            struct timespec ts = {};
                            clock_gettime(CLOCK_REALTIME, &ts);
                            std::int64_t milliseconds = ((ts.tv_sec*1000) + (ts.tv_nsec/1000000));
                            ctx_ptr->start_time() = milliseconds;

                            // The second resume executes the function and collects the results.
                            ctx_ptr->resume();

                            clock_gettime(CLOCK_REALTIME, &ts);
                            milliseconds = ((ts.tv_sec*1000) + (ts.tv_nsec/1000000));
                            ctx_ptr->end_time() = milliseconds;
                            mbox_ptr->signal.fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                            mbox_ptr->mbx_cv.notify_all();
                            ctx_ptr->stop_thread();
                        }, controller_mbox_ptr_
                    );
                    ctx_ptr->tid() = executor.native_handle();
                    ctx_ptr->req() = req;
                    ctx_ptrs.push_back(std::move(ctx_ptr));
                    executor.detach();
                } else if (req.route == "/init" ) {
                    if ( initialized_ ){
                        Http::Response res = {
                            .status_code = "409",
                            .status_message = "Conflict",
                            .connection = "close",
                            .content_length = 0,
                            .body = ""
                        };
                        std::stringstream ss;
                        ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                           << "Connection: " << res.connection << "\r\n"
                           << "Content-Type: application/json\r\n"
                           << "Content-Length: " << res.content_length << "\r\n"
                           << "\r\n";
                        auto session_it = std::find_if(server_.http_sessions().begin(), server_.http_sessions().end(), [&](auto session){ return session.request() == req; });
                        if ( session_it != server_.http_sessions().end() ){
                            std::string str(ss.str());
                            //Write the http response to the unix socket with a unique fd.
                            std::unique_lock<std::mutex> lk(controller_mbox_ptr_->mbx_mtx);
                            controller_mbox_ptr_->mbx_cv.wait(lk, [&](){ return ((controller_mbox_ptr_->signal.load() & echo::Signals::APP_UNIX_WRITE) == 0); });
                            controller_mbox_ptr_->payload_buffer_ptr = std::make_shared<std::vector<char> >(str.begin(), str.end());
                            controller_mbox_ptr_->session_ptr = std::shared_ptr<UnixServer::Session>(session_it->unix_session());
                            lk.unlock();
                            controller_mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                            controller_mbox_ptr_->signal.fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                            controller_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                            server_.http_sessions().erase(session_it);                            
                        }
                    } else {
                        initialized_ = true;
                        controller::resources::init::Request init(val.get_object());
                        Http::Response res = controller::resources::init::handle(init);
                        std::stringstream ss;
                        ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                            << "Connection: " << res.connection << "\r\n"
                            << "Content-Type: application/json\r\n"
                            << "Content-Length: " << res.content_length << "\r\n"
                            << "\r\n";
                        auto session_it = std::find_if(server_.http_sessions().begin(), server_.http_sessions().end(), [&](auto session){ return session.request() == req; });
                        if ( session_it != server_.http_sessions().end() ){
                            std::string str(ss.str());
                            //Write the http response to the unix socket with a unique fd.
                            std::unique_lock<std::mutex> lk(controller_mbox_ptr_->mbx_mtx);
                            controller_mbox_ptr_->mbx_cv.wait(lk, [&](){ return ((controller_mbox_ptr_->signal.load() & echo::Signals::APP_UNIX_WRITE) == 0); });
                            controller_mbox_ptr_->payload_buffer_ptr = std::make_shared<std::vector<char> >(str.begin(), str.end());
                            controller_mbox_ptr_->session_ptr = std::shared_ptr<UnixServer::Session>(session_it->unix_session());
                            lk.unlock();
                            controller_mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                            controller_mbox_ptr_->signal.fetch_or(echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                            controller_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                            server_.http_sessions().erase(session_it);                            
                        }
                    }
                }
            } catch ( std::bad_alloc const& e){
                #ifdef DEBUG
                std::cout << "Parsing Failed: " << e.what() << std::endl;
                #endif
            }
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
                .annotations = { "ANNOTATION:1", "ANNOTATION:2" },
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
                .connection = "close",
                .content_length = ss.str().size(),
                .body = ss.str(),
            };
            return res;
        } else {
            throw;
        }
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