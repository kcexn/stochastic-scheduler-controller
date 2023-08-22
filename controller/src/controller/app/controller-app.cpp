#include "controller-app.hpp"
#include "../resources/run/run.hpp"
#include <boost/context/fiber.hpp>
#include <thread>
#include <boost/json.hpp>
#include <functional>

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
            controller_mbox_ptr_->mbx_cv.wait(lk, [&]{ return (controller_mbox_ptr_->msg_flag == true || controller_mbox_ptr_->signal.load() != 0); });
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
                    boost::context::fiber f = controller::resources::run::handle(run);
                    std::shared_ptr<ExecutionContext> ctx_ptr = std::make_shared<ExecutionContext>(std::move(f));
                    ctx_ptrs.push_back(std::move(ctx_ptr));
                    while( *(ctx_ptrs.back()) ){
                        ctx_ptrs.back()->resume();
                    }

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