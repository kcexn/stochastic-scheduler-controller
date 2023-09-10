#include "controller-app.hpp"
#include "../resources/resources.hpp"
#include <functional>
#include <ctime>
#include <unistd.h>

#ifdef DEBUG
#include <iostream>
#endif
namespace controller{
namespace app{
    Controller::Controller(std::shared_ptr<echo::MailBox> mbox_ptr, boost::asio::io_context& ioc)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<echo::MailBox>()),
        io_(io_mbox_ptr_, "/run/controller/controller.sock", ioc)
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
            //Unset all of the scheduler signals except TERMINATE.
            io_mbox_ptr_->sched_signal_ptr->fetch_and( ~(thread_local_signal & ~echo::Signals::TERMINATE), std::memory_order::memory_order_relaxed);
            thread_local_msg_flag = io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed);
            // Unset the msg box flag.
            io_mbox_ptr_->msg_flag.store(false, std::memory_order::memory_order_relaxed);
            Http::Session http_session( io_mbox_ptr_->session );
            lk.unlock();

            #ifdef DEBUG
            std::cout << "HTTP Server Loop Start!" << std::endl;
            #endif

            if (thread_local_msg_flag){
                if (( thread_local_signal & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                    pthread_exit(0);
                }
                auto it = std::find(server_.begin(), server_.end(), http_session);
                if (it != server_.end()){
                    // Do Something.
                    #ifdef DEBUG
                    std::cout << "Session is in the HTTP Server." << std::endl;
                    #endif
                    route_request(it->read_request());
                } else {
                    #ifdef DEBUG
                    std::cout << "Session is not in the HTTP Server." << std::endl;
                    #endif
                    server_.push_back(std::move(http_session));
                    route_request(server_.back().read_request());

                }
                #ifdef DEBUG
                std::cout << "HTTP Server Loop End!" << std::endl;
                #endif
            }
            if ( (thread_local_signal & echo::Signals::SCHED_END) == echo::Signals::SCHED_END ){
                // Find a context that has a valid stopped thread.
                auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](std::shared_ptr<ExecutionContext>& ctx_ptr){
                    auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](ThreadControls& thread){
                        return thread.is_stopped() && thread.is_valid();
                    });
                    return (tmp == ctx_ptr->thread_controls().end())? false : true;
                });

                // While there are stopped threads.
                while(it != ctx_ptrs.end()){
                    // Check to see if the context is stopped.
                    if ((*it)->is_stopped()){
                        // create the response.
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
                        for ( auto& req: (*it)->reqs() ){
                            auto session_it = std::find_if(server_.begin(), server_.end(), [&](auto session){ return session.request() == req; });
                            if (session_it != server_.end()){
                                //Write the http response to the unix socket with a unique fd.
                                std::string str(ss.str());
                                boost::asio::const_buffer write_buffer(str.data(), str.size());
                                std::shared_ptr<server::Session>& session = session_it->session();
                                session_it->session()->async_write(
                                    write_buffer,
                                    [&, session](){
                                        session->cancel();
                                        session->close();
                                        session->erase();
                                    }
                                );
                                server_.erase(session_it);
                            } // else the request is no longer in the http sessions table, so we erase the context, and do nothing.
                        }
                        ctx_ptrs.erase(it); // This invalidates the iterator in the loop, so we have to perform the original search again.
                        // Find a context that has a stopped thread.
                        it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& ctx_ptr){
                            auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](auto& thread){
                                return thread.is_stopped() && thread.is_valid();
                            });
                            return (tmp == ctx_ptr->thread_controls().end())? false : true;
                        });
                        flush_wsk_logs();
                    } else {
                        // Evaluate which thread to execute next and notify it.
                        auto stopped_thread = std::find_if((*it)->thread_controls().begin(), (*it)->thread_controls().end(), [&](auto& thread){
                            return thread.is_stopped() && thread.is_valid();
                        });
                        // invalidate the thread.
                        std::vector<std::size_t> execution_context_idxs = stopped_thread->invalidate();
                        // get the index of the stopped thread.
                        std::ptrdiff_t idx = stopped_thread - (*it)->thread_controls().begin();
                        // Get the key of the action at this index+1 (mod thread_controls.size())
                        std::string key((*it)->manifest()[(++idx)%((*it)->thread_controls().size())]->key());
                        for (auto& idx: execution_context_idxs){
                            // Get the next relation to execute from the dependencies of the relation at this key.
                            std::shared_ptr<Relation> next = (*it)->manifest().next(key, idx);
                            // Retrieve the index of this relation in the manifest.
                            auto next_it = std::find_if((*it)->manifest().begin(), (*it)->manifest().end(), [&](auto& rel){
                                return rel->key() == next->key();
                            });
                            std::ptrdiff_t next_idx = next_it - (*it)->manifest().begin();
                            //Start the thread at this index.
                            (*it)->thread_controls()[next_idx].notify(idx);
                        }
                        // Search through remaining contexts.
                        it = std::find_if(it, ctx_ptrs.end(), [&](auto& ctx_ptr){
                            auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](auto& thread){
                                return thread.is_stopped() && thread.is_valid();
                            });
                            return (tmp == ctx_ptr->thread_controls().end())? false : true;
                        });
                    }
                }
            }
        }
        #ifdef DEBUG
        std::cout << "Controller Stop!" << std::endl;
        #endif
        pthread_exit(0);
    }

    void Controller::route_request(const std::shared_ptr<Http::Request>& req){
        #ifdef DEBUG
        std::cout << "Request Router!" << std::endl;
        #endif
        if (req->body_fully_formed){
            /* Lets leave this logging in here for now, just until I'm confident that I have the action interface implemented properly. */
            // std::ofstream log("/var/log/controller/request.log", std::ios_base::out | std::ios_base::app );
            // std::stringstream ss;
            // std::cout << req.verb << " " << req.route << " HTTP/1.0\r\n"
            //    << "Content-Length: " << req.content_length << "\r\n"
            //    << "\r\n" << req.body << "\r\n";
            // std::cout << ss.str() << std::endl;;
            /* ------------------------------------------------------------------------------------------------- */
            try{
                boost::json::error_code ec;
                boost::json::value val = boost::json::parse(req->body, ec);
                if (ec) {
                    #ifdef DEBUG
                    std::cout << "Parsing failed: " << ec.message() << std::endl;
                    #endif
                }
                // Route the request.
                if (req->route == "/run" ){
                    controller::resources::run::Request run(val.as_object());
                    // Create a fiber continuation for processing the request.
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run, ctx_ptrs); 
                    ctx_ptr->reqs().push_back(std::shared_ptr<Http::Request>(req));
                    if ( initialized_ ){
                        // Initialize threads only once.
                        // If ctx_ptr is already in the controller ctx_ptrs then threads don't need to be initialized again.
                        auto ctx_it = std::find(ctx_ptrs.begin(), ctx_ptrs.end(), ctx_ptr);
                        if (ctx_it == ctx_ptrs.end()){
                            ctx_ptrs.push_back(ctx_ptr);
                            for( int i = 0; i < ctx_ptr->manifest().size(); ++i ){
                                std::thread executor(
                                    [&, ctx_ptr, i](std::shared_ptr<echo::MailBox> mbox_ptr){
                                        #ifdef DEBUG
                                        std::cout << "Index: " << i << std::endl;
                                        #endif

                                        pthread_t tid = pthread_self();
                                        ctx_ptr->thread_controls()[i].tid() = tid;
                                        #ifdef DEBUG
                                        std::cout << "Thread ID: " << tid << " Pre Fiber acquisition!" << std::endl;
                                        #endif
                                        // The first resume sets up the action runtime environment for execution.
                                        // The action runtime doesn't have to be set up in a distinct 
                                        // thread of execution, but since we need to take the time to set up 
                                        // a thread anyway, deferring the process fork in the execution context until after the 
                                        // thread is established so that the fork can happen concurrently 
                                        // is a more performant solution.
                                        ctx_ptr->thread_controls()[i].resume();
                                        
                                        #ifdef DEBUG
                                        std::cout << "Thread ID: " << tid << " Post fiber acquisition!" << std::endl;
                                        #endif

                                        ctx_ptr->thread_controls()[i].wait();

                                        #ifdef DEBUG
                                        std::cout << "Thread ID: " << tid << ", SCHED_START Notified!" << std::endl;
                                        #endif

                                        ctx_ptr->thread_controls()[i].resume();
                                        ctx_ptr->thread_controls()[i].signal().fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                                        mbox_ptr->sched_signal_ptr->fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                                        mbox_ptr->sched_signal_cv_ptr->notify_all();
                                    }, io_mbox_ptr_
                                );
                                executor.detach();
                            }
                        }
                        // This id is pushed in the context constructor.
                        std::size_t execution_idx = ctx_ptr->pop_execution_idx();
                        std::size_t manifest_size = ctx_ptr->manifest().size();
                        // Get the starting relation.
                        std::string start_key(ctx_ptr->manifest()[execution_idx % manifest_size]->key());
                        #ifdef DEBUG
                        std::cout << "Starting Key: " << start_key << std::endl;
                        #endif

                        std::shared_ptr<Relation> start = ctx_ptr->manifest().next(start_key, execution_idx);
                        #ifdef DEBUG
                        std::cout << "Next Key: " << start->key() << std::endl;
                        #endif

                        // Find the index in the manifest of the starting relation.
                        auto start_it = std::find_if(ctx_ptr->manifest().begin(), ctx_ptr->manifest().end(), [&](auto& rel){
                            return rel->key() == start->key();
                        });
                        if (start_it == ctx_ptr->manifest().end()){
                            // If the start key is past the end of the manifest, that means that
                            // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                            io_mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::SCHED_END, std::memory_order::memory_order_relaxed);
                            io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                            return;
                        }
                        std::ptrdiff_t start_idx = start_it - ctx_ptr->manifest().begin();
                        #ifdef DEBUG
                        std::cout << "Select Index: " << start_idx << std::endl;
                        std::cout << "Execution Index: " << execution_idx << std::endl;
                        #endif
                        ctx_ptr->thread_controls()[start_idx].notify(execution_idx);
                    } else {
                        // invalidate the fibers.
                        for (auto& thread_control: ctx_ptr->thread_controls() ){
                            thread_control.invalidate_fiber();
                        }
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
                            std::shared_ptr<server::Session>& session = session_it->session();
                            session->async_write(
                                write_buffer,
                                [&, session](){
                                    session->cancel();
                                    session->erase();
                                    session->close();
                                }
                            );
                            server_.erase(session_it);
                        }
                    }
                } else if (req->route == "/init" ) {
                    controller::resources::init::Request init(val.as_object());
                    // It is not strictly necessary to construct a context for initialization requests.
                    // But it keeps the controller resource interface homogeneous and easy to follow.
                    // Also, since the initialization route is only called once, the cost to performance
                    // should not be significant.
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::init::handle(init);
                    ctx_ptr->reqs().push_back(std::shared_ptr<Http::Request>(req));
                    Http::Response res = {};
                    if ( initialized_ ) {
                        // invalidate the fibers.
                        for (auto& thread_control: ctx_ptr->thread_controls()){
                            thread_control.invalidate_fiber();
                        }
                        res = create_response(*ctx_ptr);
                    } else {
                        // Execute the initializer.
                        res = create_response(*ctx_ptr);
                        if ( res.status_code == "200" ){
                            initialized_ = true;
                            for(auto& thread_control: ctx_ptr->thread_controls()){
                                thread_control.resume();
                            }
                        }   
                    }
                    std::stringstream ss;
                    ss << "HTTP/1.0 " << res.status_code << " " << res.status_message << "\r\n"
                        << "Connection: " << res.connection << "\r\n"
                        << "Content-Type: application/json\r\n"
                        << "Content-Length: " << res.content_length << "\r\n"
                        << "\r\n";
                    auto it = std::find_if(server_.begin(), server_.end(), [&](auto session){ return session.request() == req; });
                    if(it != server_.end()){
                        std::string str(ss.str());
                        //Write the http response to the unix socket with a unique fd.
                        boost::asio::const_buffer write_buffer(str.data(), str.size());
                        std::shared_ptr<server::Session>& session = it->session();
                        session->async_write(
                            write_buffer,
                            [&, session](){
                                session->cancel();
                                session->close();
                                session->erase();
                            }
                        );
                        server_.erase(it);
                    }
                }
            } catch ( std::bad_alloc const& e){
                #ifdef DEBUG
                std::cout << "Parsing Failed: " << e.what() << std::endl;
                #endif
                throw e;
            }
        }
    }

    Http::Response Controller::create_response(ExecutionContext& ctx){
        Http::Response res = {};
        if ( ctx.reqs()[0]->route == "/run" ){
            if ( ctx.is_stopped() ){
                boost::json::object jv;
                bool ec{false};
                const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
                if ( __OW_ACTIONS == nullptr ){
                    throw "environment variable __OW_ACTIONS is not defined.";
                }
                std::filesystem::path path(__OW_ACTIONS);
                std::filesystem::path manifest_path(path/"action-manifest.json");
                bool manifest_exists = std::filesystem::exists(manifest_path);
                for ( auto& relation: ctx.manifest() ){
                    boost::json::error_code err;
                    std::string value = relation->acquire_value();
                    relation->release_value();
                    boost::json::object val = boost::json::parse(value, err).as_object();
                    auto it = std::find_if(val.begin(), val.end(), [&](auto kvp){ return kvp.key() == "error"; });
                    if ( it != val.end() ){
                        ec = true;
                    }
                    if ( manifest_exists ){
                        jv.emplace(relation->key(), val);
                    } else {
                        jv = val;
                    }
                }
                std::string ss = boost::json::serialize(jv);
                if ( !ec ){
                    res = {
                        "200",
                        "OK",
                        "close",
                        ss.size(),
                        ss
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
        } else if ( ctx.reqs()[0]->route == "/init" ){
            if ( ctx.is_stopped() ) {
                res = {
                    "409",
                    "Conflict",
                    "close",
                    0,
                    ""
                };
            } else {
                boost::json::value jv = boost::json::parse(ctx.reqs()[0]->body);
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