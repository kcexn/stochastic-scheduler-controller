#include "controller-app.hpp"
#include "../../echo-app/utils/common.hpp"
#include <application-servers/http/http-session.hpp>
#include "execution-context.hpp"
#include "action-relation.hpp"
#include "../resources/resources.hpp"

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
            std::shared_ptr<server::Session> server_session(std::move(io_mbox_ptr_->session));
            lk.unlock();

            // Http::Session http_session( server_session );

            std::shared_ptr<http::HttpSession> http_session_ptr;
            auto http_session_it = std::find_if(hs_.begin(), hs_.end(), [&](auto& ptr){
                return *ptr == server_session;
            });
            if( http_session_it == hs_.end() ){
                #ifdef DEBUG
                std::cout << "Http Session pushed back onto hs_." << std::endl;
                #endif
                http_session_ptr = std::make_shared<http::HttpSession>(hs_, server_session);
                hs_.push_back(http_session_ptr);
            } else {
                // For our application all session pointers are http session pointers.
                http_session_ptr = std::static_pointer_cast<http::HttpSession>(*http_session_it);
            }
            #ifdef DEBUG
            std::cout << "HTTP Server Loop Start!" << std::endl;
            #endif

            if (thread_local_msg_flag){
                if (( thread_local_signal & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                    pthread_exit(0);
                }
                http_session_ptr->read();
                route_request(http_session_ptr);
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
                        boost::json::value val;
                        while((*it)->sessions().size() > 0 ){
                            std::shared_ptr<http::HttpSession> next_session = (*it)->sessions().back();
                            http::HttpReqRes rr;
                            std::get<http::HttpResponse>(rr) = create_response(**it, val);
                            next_session->write(
                                rr,
                                [&, next_session](){
                                    next_session->close();
                                }
                            );
                            (*it)->sessions().pop_back();
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

    void Controller::route_request(std::shared_ptr<http::HttpSession>& session){
        #ifdef DEBUG
        std::cout << "Request Router!" << std::endl;
        #endif
        http::HttpReqRes req_res = session->get();
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);
        #ifdef DEBUG
        std::cout << req << std::endl;
        #endif
        // Start processing chunks, iff next_chunk > 0.
        if(req.next_chunk > 0){
            if(req.route == "/run" || req.route == "/init"){
                // For our application, each chunk that is emitted on the HTTP stream must be a fully 
                // formed and correct JSON object. There are two possible options for the arrival of data on the HTTP stream.
                // 1) An array of JSON objects e.g.;
                //     [{...}, {...}, {...}]
                // 2) A singleton list of JSON objects, which has two possible representations.
                //     [{...}]
                //     {...}
                // importantly, if the HTTP stream contains an array, we must be able to strip leading and trailing brackets, and commas before 
                // parsing the JSON object. Otherwise, we can just directly parse the JSON object.
                // There is no constraint on how many JSON objects can be emitted in each HTTP chunk, and so if an array of JSON objects is 
                // being emitted, we must be able to handle parsing multiple objects in one chunk that is midwday through the stream, i.e.:
                // {...}, {...}, {...}
                // with no leading or trailing brackets.
                // Finally, the closing brace that we need to look for is the closing brace of the entire JSON object, and not of 
                // any nested objects. So we must search for the closing brace from the comma delimiter position in reverse.
                for(; req.pos < req.next_chunk; ++req.pos){
                    std::size_t next_comma = 0;
                    const http::HttpChunk& chunk = req.chunks[req.pos];
                    const std::size_t chunk_size = chunk.chunk_data.size();
                    while(next_comma < chunk_size){
                        // Count the number of unclosed braces.
                        std::size_t brace_count = 0;
                        // Track the start and end positions of 
                        // the top level javascript object.
                        std::size_t next_opening_brace = 0;
                        std::size_t next_closing_brace = 0;
                        for(; next_comma < chunk_size; ++next_comma){
                            const char& c = chunk.chunk_data[next_comma];
                            if(c == '}'){
                                // track the last found closing brace as we go through the loop.
                                --brace_count;
                                if(brace_count == 0){
                                    next_closing_brace = next_comma;
                                }
                            }

                            if(c == '{'){
                                if(brace_count == 0){
                                    next_opening_brace = next_comma;
                                }
                                ++brace_count;
                            } else if (brace_count == 0 && c == ','){
                                // Once we find a top level comma, then we know that we have reached the end of 
                                // a javascript object.
                                break;
                            }
                        }
                        boost::json::error_code ec;
                        boost::json::value val = boost::json::parse(
                            chunk.chunk_data.substr(next_opening_brace, next_closing_brace - next_opening_brace + 1), 
                            ec
                        );
                        if(ec){throw "Json Parsing failed.";}
                        if(req.route == "/run"){
                            controller::resources::run::Request run(val.as_object());
                            // Create a fiber continuation for processing the request.
                            std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run, ctx_ptrs); 
                            auto http_it = std::find(ctx_ptr->sessions().cbegin(), ctx_ptr->sessions().cend(), session);
                            if(http_it == ctx_ptr->sessions().cend()){
                                ctx_ptr->sessions().push_back(session);
                            }
                            if (initialized_){
                                // Initialize threads only once.
                                // If ctx_ptr is already in the controller ctx_ptrs then threads don't need to be initialized again.
                                auto ctx_it = std::find(ctx_ptrs.begin(), ctx_ptrs.end(), ctx_ptr);
                                if (ctx_it == ctx_ptrs.end()){
                                    ctx_ptrs.push_back(ctx_ptr);
                                    const std::size_t& manifest_size = ctx_ptr->manifest().size();
                                    for( std::size_t i = 0; i < manifest_size; ++i ){
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
                                http::HttpReqRes rr;
                                while(ctx_ptr->sessions().size() > 0)
                                {
                                    std::shared_ptr<http::HttpSession>& next_session = ctx_ptr->sessions().back();
                                    std::get<http::HttpResponse>(rr) = create_response(*ctx_ptr, val);
                                    next_session->write(
                                        rr,
                                        [&, next_session](){
                                            next_session->close();
                                        }
                                    );
                                    ctx_ptr->sessions().pop_back();
                                }
                            }
                        } else if (req.route == "/init"){
                            controller::resources::init::Request init(val.as_object());
                            // It is not strictly necessary to construct a context for initialization requests.
                            // But it keeps the controller resource interface homogeneous and easy to follow.
                            // Also, since the initialization route is only called once, the cost to performance
                            // should not be significant.
                            std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::init::handle(init);
                            auto http_it = std::find_if(ctx_ptr->sessions().cbegin(), ctx_ptr->sessions().cend(), [&](auto& ptr){
                                return ptr == session;
                            });
                            if(http_it == ctx_ptr->sessions().cend()){
                                ctx_ptr->sessions().push_back(session);
                            }
                            http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
                            res = create_response(*ctx_ptr, val);
                            if ( initialized_ ) {
                                // invalidate the fibers.
                                for (auto& thread_control: ctx_ptr->thread_controls()){
                                    thread_control.invalidate_fiber();
                                }
                            } else {
                                // Execute the initializer.
                                if ( res.status == http::HttpStatus::OK ){
                                    initialized_ = true;
                                    for(auto& thread_control: ctx_ptr->thread_controls()){
                                        thread_control.resume();
                                    }
                                }
                            }
                            session->write(
                                req_res,
                                [&, session](){
                                    session->close();
                                }
                            );         
                        }
                    }
                }
            } else {
                http::HttpReqRes rr;
                http::HttpResponse res = {
                    req.version,
                    http::HttpStatus::NOT_FOUND,
                    {
                        {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        {http::HttpHeaderField::CONNECTION, "close"},
                        {http::HttpHeaderField::END_OF_HEADERS, ""}
                    },
                    {
                        {{0}, ""}
                    }
                };
                std::get<http::HttpResponse>(rr) = res;
                session->write(
                    rr,
                    [&, session](){
                        session->close();
                    }
                );
            }
        }
    }

    http::HttpResponse Controller::create_response(ExecutionContext& ctx, boost::json::value& val){
        http::HttpResponse res = {};
        if(ctx.route() == controller::resources::Routes::RUN){
            // The run route is not necessarily single threaded so we must use threadsafe get.
            std::shared_ptr<http::HttpSession> session = ctx.sessions().back();
            http::HttpReqRes rr = session->get();
            http::HttpRequest& req = std::get<http::HttpRequest>(rr);
            if(!initialized_){
                res = {
                    req.version,
                    http::HttpStatus::NOT_FOUND,
                    {
                        {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        {http::HttpHeaderField::CONNECTION, "close"},
                        {http::HttpHeaderField::END_OF_HEADERS, ""}                            
                    },
                    {
                        {{0},""}
                    }
                };
            } else if (ctx.is_stopped()){
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
                std::stringstream len;
                len << ss.size();
                if(!ec){
                    res = {
                        req.version,
                        http::HttpStatus::OK,
                        {
                            {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                            {http::HttpHeaderField::CONTENT_LENGTH, len.str()},
                            {http::HttpHeaderField::CONNECTION, "close"},
                            {http::HttpHeaderField::END_OF_HEADERS, ""} 
                        },
                        {
                            {{ss.size()}, ss}
                        }
                    };
                } else {
                    res = {
                        req.version,
                        http::HttpStatus::INTERNAL_SERVER_ERROR,
                        {
                            {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                            {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                            {http::HttpHeaderField::CONNECTION, "close"},
                            {http::HttpHeaderField::END_OF_HEADERS, ""} 
                        },
                        {
                            {{0}, ""}
                        }
                    };
                }
            } else {
                res = {
                    req.version,
                    http::HttpStatus::INTERNAL_SERVER_ERROR,
                    {
                        {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        {http::HttpHeaderField::CONNECTION, "close"},
                        {http::HttpHeaderField::END_OF_HEADERS, ""} 
                    },
                    {
                        {{0}, ""}
                    }
                };
            }            
        } else if (ctx.route() == controller::resources::Routes::INIT){
            // The initialization route is alwasy singled threaded, so the following
            // operation is safe.
            http::HttpRequest& req = std::get<http::HttpRequest>(*(ctx.sessions().front()));
            if (initialized_) {
                res = {
                    req.version,
                    http::HttpStatus::CONFLICT,
                    {
                        {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        {http::HttpHeaderField::CONNECTION, "close"},
                        {http::HttpHeaderField::END_OF_HEADERS, ""}
                    },
                    {
                        {{0}, ""}
                    }
                };
            } else {       
                res = {
                    req.version,
                    http::HttpStatus::OK,
                    {
                        {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        {http::HttpHeaderField::CONNECTION, "close"},
                        {http::HttpHeaderField::END_OF_HEADERS, ""}
                    },
                    {
                        {{0}, ""}
                    }
                };
            }         
        }
        // Unknown routes are handled directly at the root of the application.
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