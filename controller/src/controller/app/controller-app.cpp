#include "controller-app.hpp"
#include "../resources/resources.hpp"
#include <thread>
#include <functional>
#include <ctime>
#include <fstream>
#include <unistd.h>

#ifdef DEBUG
#include <iostream>
#endif
namespace controller{
namespace app{
    Relation::Relation(const std::string& key, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies)
      : kvp_(std::string(key), std::string() ),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(std::string&& key, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies)
      : kvp_(std::string(key), std::string() ),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(const std::string& key, const std::string& value, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies)
      : kvp_(std::string(key), std::string(value)),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(std::string&& key, std::string&& value, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies)
      : kvp_(std::string(key), std::string(value)),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    // Action Manifest.
    ActionManifest::ActionManifest()
      : index_()
    {}

    void ActionManifest::emplace(const std::string& key, const boost::json::object& manifest){
        // Search for key in index_
        auto it = std::find_if(index_.begin(), index_.end(),[&](auto& rel){
            return rel->key() == key;
        });
        // If the key isn't in the manifest, loop through
        // all of the dependencies, and recursively insert them.
        if ( it == index_.end() ){
            boost::json::array deps(manifest.at(key).as_object().at("depends").as_array());
            std::vector<std::shared_ptr<Relation> > dependencies;
            for (auto& dep: deps){
                std::string dep_key(dep.as_string());
                emplace(dep_key, manifest);
                // Find the emplaced key in the index.
                auto tmp = std::find_if(index_.begin(), index_.end(), [&](auto& rel){
                    return rel->key() == dep_key;
                });
                // Create a copy of the std::shared_ptr<Relation> and emplace 
                // it into the dependencies vector.
                dependencies.emplace_back(*tmp);
            }
            // This is the second recursive base case.
            // All of the dependencies have been emplaced, so this element can be constructed and emplaced now.
            std::string fname(manifest.at(key).as_object().at("file").as_string());
            const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
            std::filesystem::path path(__OW_ACTIONS);
            path /= fname;
            
            std::shared_ptr<Relation> rel = std::make_shared<Relation>(key, path, dependencies);
            index_.push_back(std::move(rel));
            return;
        } else {
            // This is one of the recursive base cases. The key is already in the 
            // index, so don't do anything.
            return;
        }
    }

    std::shared_ptr<Relation> ActionManifest::next(const std::string& key, const std::size_t& idx){
        // Return the next task that needs to be completed in the list of dependences for 
        // "key".

        // Search for "key" in index.
        auto it = std::find_if(index_.begin(), index_.end(),[&](auto& rel){
            return rel->key() == key;
        });
        // If key isn't in index. Throw an exception, this shouldn't be possible.
        if (it == index_.end() ){
            throw "Key not in index!";
        }

        // Iterate through all of the dependencies starting at
        // the index: idx (mod dependencies.size()).
        std::size_t num_deps = (*it)->size();
        if (num_deps == 0){
            // The first recursive base case.
            // The number of dependencies the current relation has is 0.
            // Therefore we can just directly return the relation.
            return *it;
        }
        std::size_t start = idx % num_deps;
        for (std::size_t offset=0; offset < num_deps; ++offset){
            std::size_t select = (start + offset)%num_deps;
            // If the dependency value is empty that means the dependency
            // has not finished computing yet. Call next recursively.
            #ifdef DEBUG
            std::cout << (**it)[select]->key() << std::endl;
            #endif
            std::string value = (**it)[select]->acquire_value();
            (**it)[select]->release_value();
            if ( value.empty() ){
                std::string dep_key((**it)[select]->key());
                return next(dep_key, idx);
            }
        }
        // The second recursive base case.
        // All of the dependencies have computed values. Therefore return the current relation.
        return  *it;
    }

    std::vector<std::shared_ptr<Relation> >::iterator ActionManifest::begin() { return index_.begin(); }
    std::vector<std::shared_ptr<Relation> >::iterator ActionManifest::end() { return index_.end(); }
    std::vector<std::shared_ptr<Relation> >::const_iterator ActionManifest::cbegin() { return index_.cbegin(); }
    std::vector<std::shared_ptr<Relation> >::const_iterator ActionManifest::cend() { return index_.cend(); }
    std::vector<std::shared_ptr<Relation> >::reverse_iterator ActionManifest::rbegin() { return index_.rbegin(); }
    std::vector<std::shared_ptr<Relation> >::reverse_iterator ActionManifest::rend() { return index_.rend(); }
    std::vector<std::shared_ptr<Relation> >::const_reverse_iterator ActionManifest::crbegin() { return index_.crbegin(); }
    std::vector<std::shared_ptr<Relation> >::const_reverse_iterator ActionManifest::crend() { return index_.crend(); }

    std::shared_ptr<Relation>& ActionManifest::operator[](std::vector<std::shared_ptr<Relation> >::size_type pos){ return index_[pos]; }
    
    // Thread Controls
    void ThreadControls::wait(){
         std::unique_lock<std::mutex> lk(*mtx_); 
         cv_->wait(lk, [&](){ 
            return ((signal_->load(std::memory_order::memory_order_relaxed)&echo::Signals::SCHED_START)==echo::Signals::SCHED_START);
        }); 
        return;
    }

    void ThreadControls::notify(std::size_t idx){ 
        mtx_->lock();
        execution_context_idxs_.push_back(idx);
        mtx_->unlock();
        signal_->fetch_or(echo::Signals::SCHED_START, std::memory_order::memory_order_relaxed); 
        cv_->notify_all(); 
        return; 
    }

    std::vector<std::size_t> ThreadControls::invalidate() { 
        valid_->store(false, std::memory_order::memory_order_relaxed);  
        mtx_->lock();
        std::vector<std::size_t> tmp(execution_context_idxs_.size());
        std::memcpy(tmp.data(), execution_context_idxs_.data(), execution_context_idxs_.size());
        mtx_->unlock();
        return tmp;
    }

    // Execution Context
    ExecutionContext::ExecutionContext(ExecutionContext::Init init)
      : execution_context_id_(UUID::uuid_create_v4()),
        execution_context_idx_stack_{0}
    {}

    ExecutionContext::ExecutionContext(ExecutionContext::Run run)
      : execution_context_id_(UUID::uuid_create_v4()),
        execution_context_idx_stack_{0}
    {
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            throw "Environment variable __OW_ACTIONS not defined!";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            boost::json::object manifest(tmp.as_object());
            if (ec){
                throw "boost json parse failed.";
            }
            #ifdef DEBUG
            std::cout << manifest << std::endl;
            #endif
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
                throw "action-manifest.json can't be empty.";
            }
            // Loop through manifest.json until manifest_ contains the same number of keys.
            while(manifest_.size() < manifest.size()){
                #ifdef DEBUG
                std::cout << manifest_.size() << std::endl;
                std::cout << manifest.size() << std::endl;
                #endif
                // Find a key that isn't in the manifest_ yet.
                auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
                    auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
                        return rel->key() == kvp.key();
                    });
                    return (tmp != manifest_.end()) ? false : true;
                });
                // Insert it into the manifest, using a recursive tree traversal method.
                std::string key(it->key());
                manifest_.emplace(key, manifest); 
            }
            #ifdef DEBUG
            std::cout << manifest.size() << std::endl;
            #endif
            // Reverse lexicographically sort the manifest.
            std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
                return a->depth() > b->depth();
            });
        } else {
            const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
            if ( __OW_ACTION_EXT == nullptr ){
                throw "Environment variable __OW_ACTION_EXT is not defined!";
            }
            // By default the file is called "main" + __OW_ACTION_EXT.
            // e.g. "main.lua", or "main.py", or "main.js".
            std::string filename("main");
            filename.append(".");
            filename.append(__OW_ACTION_EXT);
            std::filesystem::path fn_path(action_path / filename);

            const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
            std::string entrypoint;
            if ( __OW_ACTION_ENTRY_POINT == nullptr ){
                // By default, the entry point is called main.
                entrypoint = std::string("main");
            } else {
                entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
            }
            // By default, the entry point has no dependencies.
            manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
        }
    }

    // ExecutionContext::ExecutionContext(ExecutionContext::Run run, UUID::uuid_t execution_context_id, std::size_t execution_context_idx)
    //   : execution_context_id_(execution_context_id),
    //     execution_context_index_(execution_context_idx)
    // {
    //     const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
    //     if ( __OW_ACTIONS == nullptr ){
    //         throw "Environment variable __OW_ACTIONS not defined!";
    //     }
    //     std::filesystem::path action_path(__OW_ACTIONS);
    //     std::filesystem::path manifest_path(action_path / "action-manifest.json");
    //     if (std::filesystem::exists(manifest_path)){
    //         std::fstream f(manifest_path, std::ios_base::in);
    //         boost::json::error_code ec;
    //         boost::json::value tmp = boost::json::parse(f,ec);
    //         boost::json::object manifest(tmp.as_object());
    //         if (ec){
    //             throw "boost json parse failed.";
    //         }
    //         #ifdef DEBUG
    //         std::cout << manifest << std::endl;
    //         #endif
    //         // If the manifest is empty throw an exception.
    //         if(manifest.empty()){
    //             throw "action-manifest.json can't be empty.";
    //         }
    //         // Loop through manifest.json until manifest_ contains the same number of keys.
    //         while(manifest_.size() < manifest.size()){
    //             #ifdef DEBUG
    //             std::cout << manifest_.size() << std::endl;
    //             std::cout << manifest.size() << std::endl;
    //             #endif
    //             // Find a key that isn't in the manifest_ yet.
    //             auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
    //                 auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
    //                     return rel->key() == kvp.key();
    //                 });
    //                 return (tmp != manifest_.end()) ? false : true;
    //             });
    //             // Insert it into the manifest, using a recursive tree traversal method.
    //             std::string key(it->key());
    //             manifest_.emplace(key, manifest); 
    //         }
    //         #ifdef DEBUG
    //         std::cout << manifest.size() << std::endl;
    //         #endif
    //         // Reverse lexicographically sort the manifest.
    //         std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
    //             return a->depth() > b->depth();
    //         });
    //     } else {
    //         const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
    //         if ( __OW_ACTION_EXT == nullptr ){
    //             throw "Environment variable __OW_ACTION_EXT is not defined!";
    //         }
    //         // By default the file is called "main" + __OW_ACTION_EXT.
    //         // e.g. "main.lua", or "main.py", or "main.js".
    //         std::string filename("main");
    //         filename.append(".");
    //         filename.append(__OW_ACTION_EXT);
    //         std::filesystem::path fn_path(action_path / filename);

    //         const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
    //         std::string entrypoint;
    //         if ( __OW_ACTION_ENTRY_POINT == nullptr ){
    //             // By default, the entry point is called main.
    //             entrypoint = std::string("main");
    //         } else {
    //             entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
    //         }
    //         // By default, the entry point has no dependencies.
    //         manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
    //     }
    // }

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs){
        return lhs.execution_context_id() == rhs.execution_context_id();
    }

    bool ExecutionContext::is_stopped() {
        std::lock_guard<std::mutex> lk(fiber_mtx_);
        for (auto& fiber: fibers_){
            if (fiber){
                return false;
            }
        }
        return true;
    }

    std::size_t ExecutionContext::pop_execution_idx() {
        if (execution_context_idx_stack_.empty()){
            throw "Execution Context idx stack shouldn't be empty when calling pop.";
        }
        std::size_t idx = execution_context_idx_stack_.back();
        execution_context_idx_stack_.pop_back();
        return idx;
    }

    void ExecutionContext::push_execution_idx(std::size_t idx){
        execution_context_idx_stack_.push_back(idx);
        return;
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
            Http::Session http_session( io_mbox_ptr_->session_ptr );
            lk.unlock();
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
                // route_request(it->request());
                #ifdef DEBUG
                std::cout << "HTTP Server Loop!" << std::endl;
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

                while( it != ctx_ptrs.end() ){
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
                    std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run); 
                    ctx_ptr->req() = std::shared_ptr<Http::Request>(req);
                    if ( initialized_ ){
                        // Initialize threads only once.
                        if(ctx_ptr->thread_controls().empty()){
                            std::size_t num_fibers = 0;
                            {
                                // The anonymous scope is to ensure the reference is invalidated.
                                std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                                num_fibers = fibers.size();
                                ctx_ptr->release_fibers();
                            }
                            if ( ctx_ptr->manifest().size() != num_fibers ){
                                throw "Execution Context does not have an equal number of fibers and manifest elements.";
                            }
                            for( int i = 0; i < ctx_ptr->manifest().size(); ++i ){
                                ctx_ptr->thread_controls().emplace_back();
                                std::thread executor(
                                    [&, ctx_ptr, i](std::shared_ptr<echo::MailBox> mbox_ptr){
                                        ctx_ptr->thread_controls()[i].tid() = pthread_self();
                                        
                                        // The first resume sets up the action runtime environment for execution.
                                        // The action runtime doesn't have to be set up in a distinct 
                                        // thread of execution, but since we need to take the time to set up 
                                        // a thread anyway, deferring the process fork in the execution context until after the 
                                        // thread is established so that the fork can happen concurrently 
                                        // is a more performant solution.
                                        {
                                            // The anonymous scope is to ensure that the reference is immediately
                                            // invalidated after releasing the fibers.
                                            std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                                            fibers[i] = std::move(fibers[i]).resume();
                                            ctx_ptr->release_fibers();
                                        }

                                        // The second resume executes the function and collects the results.
                                        ctx_ptr->thread_controls()[i].wait();
                                        {
                                            std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                                            fibers[i] = std::move(fibers[i]).resume();
                                            ctx_ptr->release_fibers();
                                        }
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
                        std::shared_ptr<Relation> start = ctx_ptr->manifest().next(start_key, execution_idx);
                        // Find the index in the manifest of the starting relation.
                        auto start_it = std::find_if(ctx_ptr->manifest().begin(), ctx_ptr->manifest().end(), [&](auto& rel){
                            return rel->key() == start->key();
                        });
                        std::ptrdiff_t start_idx = start_it - ctx_ptr->manifest().begin();
                        ctx_ptr->thread_controls()[start_idx].notify(execution_idx);
                        // add the ctx pointer to the list of context pointers iff it does not exist.
                        auto ctx_it = std::find(ctx_ptrs.begin(), ctx_ptrs.end(), ctx_ptr);
                        if (ctx_it == ctx_ptrs.end()){
                            ctx_ptrs.push_back(std::move(ctx_ptr));
                        }
                    } else {
                        // invalidate the fibers.
                        {
                            // The anonymous scope is to ensure that the reference is invalidated.
                            std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                            fibers.clear();
                            ctx_ptr->release_fibers();
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
                            io_.async_unix_write(write_buffer, session_it->unix_session(), 
                                [&](UnixServer::Session& unix_session){
                                    unix_session.cancel_reads();
                                    unix_session.shutdown_write();
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
                    ctx_ptr->req() = std::shared_ptr<Http::Request>(req);
                    Http::Response res = {};
                    if ( initialized_ ) {
                        // invalidate the fibers.
                        {
                            // The anonymous scope is to ensure that the reference is invalidated.
                            std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                            fibers.clear();
                            ctx_ptr->release_fibers();
                        }
                        res = create_response(*ctx_ptr);
                    } else {
                        // Execute the initializer.
                        res = create_response(*ctx_ptr);
                        if ( res.status_code == "200" ){
                            initialized_ = true;
                            {
                                // The anonymous scope is to ensure that the reference is invalidated.
                                std::vector<boost::context::fiber>& fibers = ctx_ptr->acquire_fibers();
                                fibers[0] = std::move(fibers[0]).resume();
                                ctx_ptr->release_fibers();
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
                throw e;
            }
        }
    }

    Http::Response Controller::create_response(ExecutionContext& ctx){
        Http::Response res = {};
        if ( ctx.req()->route == "/run" ){
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
        } else if ( ctx.req()->route == "/init" ){
            if ( ctx.is_stopped() ) {
                res = {
                    "409",
                    "Conflict",
                    "close",
                    0,
                    ""
                };
            } else {
                boost::json::value jv = boost::json::parse(ctx.req()->body);
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