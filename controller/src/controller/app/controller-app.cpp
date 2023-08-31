#include "controller-app.hpp"
#include "../resources/resources.hpp"
#include <boost/context/fiber.hpp>
#include <thread>
#include <boost/json.hpp>
#include <functional>
#include <ctime>
#include <fstream>
#include <unistd.h>

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
    // Controller::Controller(std::shared_ptr<echo::MailBox> mbox_ptr)
    //   : controller_mbox_ptr_(mbox_ptr),
    //     initialized_{false},
    //     io_mbox_ptr_(std::make_shared<echo::MailBox>()),
    //     io_(io_mbox_ptr_, "/run/controller/controller2.sock")
    // {
    //     #ifdef DEBUG
    //     std::cout << "Application Controller Constructor!" << std::endl;
    //     #endif
    //     // Initialize parent controls
    //     io_mbox_ptr_->sched_signal_mtx_ptr = std::make_shared<std::mutex>();
    //     io_mbox_ptr_->sched_signal_ptr = std::make_shared<std::atomic<int> >();
    //     io_mbox_ptr_->sched_signal_cv_ptr = std::make_shared<std::condition_variable>();

    //     std::thread application(
    //         &Controller::start, this
    //     );
    //     tid_ = application.native_handle();
    //     application.detach();
    // }

    Controller::Controller(std::shared_ptr<echo::MailBox> mbox_ptr, boost::asio::io_context& ioc)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<echo::MailBox>()),
        io_(io_mbox_ptr_, "/run/controller/controller2.sock", ioc)
    {
        #ifdef DEBUG
        std::cout << "Application Controller Constructor!" << std::endl;
        #endif
        // Initialize parent controls
        io_mbox_ptr_->sched_signal_mtx_ptr = std::make_shared<std::mutex>();
        io_mbox_ptr_->sched_signal_ptr = std::make_shared<std::atomic<int> >();
        io_mbox_ptr_->sched_signal_cv_ptr = std::make_shared<std::condition_variable>();

        std::thread application(
            &Controller::start, this
        );
        tid_ = application.native_handle();
        application.detach();
    }

    void Controller::start(){
        #ifdef DEBUG
        std::cout << "Controller Start!" << std::endl;
        #endif
        // Initialize resources I might need.
        std::unique_lock<std::mutex> lk(io_mbox_ptr_->mbx_mtx, std::defer_lock);
        int thread_local_signal;
        bool thread_local_msg_flag;
        // Scheduling Loop.
        // The TERMINATE signal once set, will never be cleared, so memory_order_relaxed synchronization is a sufficient check for this. (I'm pretty sure.)
        while( (io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) & echo::Signals::TERMINATE) != echo::Signals::TERMINATE ){
            lk.lock();
            io_mbox_ptr_->sched_signal_cv_ptr->wait(lk, [&]{ return (io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed) == true || io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) != 0); });
            thread_local_signal = io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed);
            thread_local_msg_flag = io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed);
            io_mbox_ptr_->msg_flag.store(false, std::memory_order::memory_order_relaxed);
            lk.unlock();
            if (thread_local_msg_flag){
                lk.lock();
                Http::Session http_session( io_mbox_ptr_->session_ptr );
                lk.unlock();
                if (( thread_local_signal & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                    pthread_exit(0);
                }
                auto it = std::find(server_.begin(), server_.end(), http_session);
                Http::Request request = {};
                if (it != server_.end()){
                    // Do Something.
                    request = it->read_request();
                    #ifdef DEBUG
                    std::cout << "Session is in the HTTP Server." << std::endl;
                    #endif
                } else {
                    server_.push_back(std::move(http_session));
                    request = server_.back().read_request();
                    #ifdef DEBUG
                    std::cout << "Session is not in the HTTP Server." << std::endl;
                    #endif
                }
                route_request(request);
                #ifdef DEBUG
                std::cout << "HTTP Server Loop!" << std::endl;
                #endif
            }
            if ( (thread_local_signal & echo::Signals::SCHED_END) == echo::Signals::SCHED_END ){
                io_mbox_ptr_->sched_signal_ptr->fetch_and(~echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
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
                    auto session_it = std::find_if(server_.begin(), server_.end(), [&](auto session){ return session.request() == (*it)->req(); });
                    if (session_it != server_.end()){
                        //Write the http response to the unix socket with a unique fd.
                        std::string str(ss.str());
                        boost::asio::const_buffer write_buffer(str.data(), str.size());
                        io_.async_unix_write(write_buffer, session_it->unix_session(), 
                            [&](UnixServer::Session& unix_session){
                                unix_session.cancel_reads();
                                unix_session.shutdown_write();
                            }
                        );
                        server_.erase(session_it);
                    } // else the request is no longer in the http sessions table, so we erase the context, and do nothing.
                    ctx_ptrs.erase(it);
                    it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [](std::shared_ptr<ExecutionContext> ctx_ptr){ return ctx_ptr->is_stopped(); });
                    flush_wsk_logs();
                }
            }
        }
        #ifdef DEBUG
        std::cout << "Controller Stop!" << std::endl;
        #endif
        pthread_exit(0);
    }

    void Controller::route_request(Http::Request& req){
        #ifdef DEBUG
        std::cout << "Request Router!" << std::endl;
        #endif
        if (req.body_fully_formed){
            /* Lets leave this logging in here for now, just until I'm confident that I have the action interface implemented properly. */
            std::ofstream log("/var/log/controller/request.log", std::ios_base::out | std::ios_base::app );
            std::stringstream ss;
            ss << req.verb << " " << req.route << " HTTP/1.0\r\n"
               << "Content-Length: " << req.content_length << "\r\n"
               << "\r\n" << req.body << "\r\n";
            log << ss.str();
            /* ------------------------------------------------------------------------------------------------- */
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
                    controller::resources::run::Request run(val.as_object());
                    // Create a fiber continuation for processing the request.
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run); 
                    ctx_ptr->req() = req;
                    if ( initialized_ ){             
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
                                mbox_ptr->sched_signal_ptr->fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                                mbox_ptr->sched_signal_cv_ptr->notify_all();
                                ctx_ptr->stop_thread();
                            }, io_mbox_ptr_
                        );
                        ctx_ptr->tid() = executor.native_handle();
                        ctx_ptrs.push_back(std::move(ctx_ptr));
                        executor.detach();
                    } else {
                        // invalidate the fiber.
                        boost::context::fiber f;
                        ctx_ptr->fiber().~fiber();
                        ctx_ptr->fiber() = std::move(f);
                        Http::Response res = create_response(*ctx_ptr);
                        std::stringstream ss;
                        ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                            << "Connection: " << res.connection << "\r\n"
                            << "Content-Type: application/json\r\n"
                            << "Content-Length: " << res.content_length << "\r\n"
                            << "\r\n"
                            << res.body;
                        auto session_it = std::find_if(server_.begin(), server_.end(), [&](auto session){ return session.request() == req; });
                        if (session_it != server_.end()){
                            std::string str(ss.str());
                            //Write the http response to the unix socket with a unique fd.
                            boost::asio::const_buffer write_buffer(str.data(), str.size());
                            io_.async_unix_write(write_buffer, session_it->unix_session(), 
                                [&](UnixServer::Session& unix_session){
                                    unix_session.cancel_reads();
                                    unix_session.shutdown_write();
                                }
                            );
                            server_.erase(session_it);
                        }
                    }
                } else if (req.route == "/init" ) {
                    controller::resources::init::Request init(val.as_object());
                    // It is not strictly necessary to construct a context for initialization requests.
                    // But it keeps the controller resource interface homogeneous and easy to follow.
                    // Also, since the initialization route is only called once, the cost to performance
                    // should not be significant.
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::init::handle(init);
                    ctx_ptr->req() = req;
                    Http::Response res = {};
                    if ( initialized_ ) {
                        // invalidate the fiber.
                        boost::context::fiber f;
                        ctx_ptr->fiber().~fiber();
                        ctx_ptr->fiber() = std::move(f);
                    } else {
                        // Execute the initializer.
                        res = create_response(*ctx_ptr);
                        if ( res.status_code == "200" ){
                            initialized_ = true;
                            ctx_ptr->resume();
                        }   

                    }
                    std::stringstream ss;
                    ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                        << "Connection: " << res.connection << "\r\n"
                        << "Content-Type: application/json\r\n"
                        << "Content-Length: " << res.content_length << "\r\n"
                        << "\r\n";
                    auto it = std::find_if(server_.begin(), server_.end(), [&](auto session){ return session.request() == req; });
                    if ( it != server_.end() ){
                        std::string str(ss.str());
                        //Write the http response to the unix socket with a unique fd.
                        boost::asio::const_buffer write_buffer(str.data(), str.size());
                        io_.async_unix_write(write_buffer, it->unix_session(), 
                            [&](UnixServer::Session& unix_session){
                                unix_session.cancel_reads();
                                unix_session.shutdown_write();
                            }
                        );
                        server_.erase(it);
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
        Http::Response res = {};
        if ( ctx.req().route == "/run" ){
            if ( !ctx && ctx.is_stopped() ){
                std::string result(ctx.payload().begin(), ctx.payload().end());
                boost::json::value jv = boost::json::parse(result);
                bool ec = false;
                auto it = std::find_if(jv.as_object().begin(), jv.as_object().end(), [&](auto kvp){ return kvp.key() == "error"; });
                if ( it != jv.as_object().end() ){
                    ec = true;
                }
                if ( !ec ){
                    res = {
                        "200",
                        "OK",
                        "close",
                        result.size(),
                        result
                    };
                } else {
                    res = {
                        "500",
                        "Internal Server Error",
                        "close",
                        0,
                        ""
                    };
                }
            } else {
               res = {
                    "404",
                    "Not Found.",
                    "close",
                    0,
                    ""
               };
            }
        } else if ( ctx.req().route == "/init" ){
            if ( !ctx ) {
                res = {
                    "409",
                    "Conflict",
                    "close",
                    0,
                    ""
                };
            } else {
                boost::json::value jv = boost::json::parse(ctx.req().body);
                bool ec = false;
                if ( jv.is_object() ){
                    boost::json::object obj = jv.get_object();
                    // Check to see if the key "value" is in the object.
                    auto it1 = std::find_if(obj.begin(), obj.end(), [&]( auto kvp ){ return kvp.key() == "value"; });
                    if ( it1 == obj.end() ){
                        ec = true;
                    } else {
                        // Check if "value" is an object.
                        if ( obj["value"].is_object() ){
                            boost::json::object value = obj["value"].get_object();
                            
                            // Check to see if the code key is in the value.
                            auto it2 = std::find_if(value.begin(), value.end(), [&]( auto kvp ){ return kvp.key() == "code"; });
                            if ( it2 == value.end() ){
                                ec = true;
                            } else {
                                boost::json::value code = it2->value();
                                // Check to see if code is a string.
                                if ( code.is_string() ){
                                    // check to see if the string size is 0.
                                    if ( code.get_string().size() == 0 ){
                                        ec = true;
                                    } else {
                                        ec = false;
                                    }
                                } else {
                                    ec = true;
                                }
                            }

                            // Check to see that the "binary" key is in the value.
                            auto it3 = std::find_if( value.begin(), value.end(), [&]( auto kvp ) { return kvp.key() == "binary"; });
                            if ( it3 == value.end() ){
                                ec = true;
                            } else {
                                boost::json::value binary = it3->value();
                                if ( binary.is_bool() ){
                                    ec = false;
                                } else {
                                    ec = true;
                                }
                            }
                        } else {
                            ec = true;
                        }
                    }
                } else {
                    ec = true;
                }

                if ( ec ){
                    res = {
                        "400",
                        "Bad Request",
                        "close",
                        0,
                        ""
                    };
                } else {
                    res = {
                        "200",
                        "OK",
                        "close",
                        0,
                        ""
                    };
                }
            }
        } else {
            throw "Unknown Route.";
        }
        return res;
    }

    void Controller::stop(){
        controller_mbox_ptr_->signal.fetch_or(echo::Signals::TERMINATE);
        controller_mbox_ptr_->mbx_cv.notify_all();
        io_mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::TERMINATE);
        io_mbox_ptr_->sched_signal_cv_ptr->notify_all();

        // Wait a reasonable amount of time for the threads to stop gracefully.
        struct timespec ts = {0,50000000};
        nanosleep(&ts, nullptr);
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