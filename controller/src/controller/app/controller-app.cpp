#include "controller-app.hpp"
#include "../controller-events.hpp"
#include <application-servers/http/http-session.hpp>
#include "execution-context.hpp"
#include "action-relation.hpp"
#include "../resources/resources.hpp"
#include <charconv>

#define CONTROLLER_APP_COMMON_HTTP_HEADERS {http::HttpHeaderField::CONTENT_TYPE, "application/json"},{http::HttpHeaderField::CONNECTION, "close"},{http::HttpHeaderField::END_OF_HEADERS, ""}

namespace controller{
namespace app{
    std::string_view find_next_json_object(const std::string& data, std::size_t& pos){
        std::string_view obj;
        // Count the number of unclosed braces.
        std::size_t brace_count = 0;
        // Count the number of opened square brackets '['.
        int bracket_count = 0;
        // Track the start and end positions of 
        // the top level javascript object.
        std::size_t next_opening_brace = 0;
        std::size_t next_closing_brace = 0;
        for(; pos < data.size(); ++pos){
            const char& c = data[pos];
            switch(c)
            {
                case '}':
                {
                    // track the last found closing brace as we go through the loop.
                    if(brace_count == 0){
                        // ignore spurious closing braces.
                        continue;
                    }
                    --brace_count;
                    if(brace_count == 0){
                        next_closing_brace = pos;
                    }
                    break;
                }
                case '{':
                {
                    if(brace_count == 0){
                        next_opening_brace = pos;
                    }
                    ++brace_count;
                    break;
                }
                case ',':
                {
                    if(next_closing_brace > next_opening_brace){
                        // Once we find a trailing top level comma, then we know that we have reached the end of 
                        // a javascript object.
                        goto exit_loop;
                    }
                    break;
                }
                case ']':
                {
                    --bracket_count;
                    if(next_closing_brace >= next_opening_brace && brace_count == 0 && bracket_count <= 0){
                        /* Function is complete. Terminate the execution context */
                        obj = std::string_view(&c, 1);
                        return obj;
                    }
                    break;
                }
                case '[':
                {
                    ++bracket_count;
                    break;
                }
            }
        }
        exit_loop: ;
        if(next_closing_brace > next_opening_brace){
            std::size_t sz = next_closing_brace - next_opening_brace + 1;
            obj = std::string_view(&data[next_opening_brace], sz);
        }
        return obj;
    }

    std::string rtostr(const server::Remote& raddr){
        std::array<char,5> pbuf;
        std::string port;
        std::to_chars_result tcres = std::to_chars(pbuf.data(), pbuf.data()+pbuf.size(), ntohs(raddr.ipv4_addr.address.sin_port), 10);
        if(tcres.ec == std::errc()){
            std::ptrdiff_t size = tcres.ptr - pbuf.data();
            port = std::string(pbuf.data(), size);
        } else {
            std::cerr << std::make_error_code(tcres.ec).message() << std::endl;
            throw "This shouldn't be possible.";
        }

        std::array<char, INET_ADDRSTRLEN> inbuf;
        const char* addr = inet_ntop(AF_INET, &raddr.ipv4_addr.address.sin_addr.s_addr, inbuf.data(), inbuf.size());

        std::string pstr(addr);
        pstr.reserve(pstr.size() + port.size() + 1);
        pstr.push_back(':');
        pstr.insert(pstr.end(), port.begin(), port.end());  
        return pstr;
    }

    Controller::Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<controller::io::MessageBox>()),
        io_(io_mbox_ptr_, "/run/controller/controller.sock", ioc)
    {
        // Initialize parent controls
        io_mbox_ptr_->sched_signal_mtx_ptr = std::make_shared<std::mutex>();
        io_mbox_ptr_->sched_signal_ptr = std::make_shared<std::atomic<std::uint16_t> >();
        io_mbox_ptr_->sched_signal_cv_ptr = std::make_shared<std::condition_variable>();

        std::thread application(
            &Controller::start, this
        );
        tid_ = application.native_handle();
        application.detach();
    }

    Controller::Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc, const std::filesystem::path& upath, std::uint16_t sport)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<controller::io::MessageBox>()),
        io_(io_mbox_ptr_, upath.string(), ioc, sport)
    {
        // Initialize parent controls
        io_mbox_ptr_->sched_signal_mtx_ptr = std::make_shared<std::mutex>();
        io_mbox_ptr_->sched_signal_ptr = std::make_shared<std::atomic<std::uint16_t> >();
        io_mbox_ptr_->sched_signal_cv_ptr = std::make_shared<std::condition_variable>();

        std::thread application(
            &Controller::start, this
        );
        tid_ = application.native_handle();
        application.detach();
    }

    void Controller::start(){
        // Initialize resources I might need.
        std::unique_lock<std::mutex> lk(io_mbox_ptr_->mbx_mtx, std::defer_lock);
        int thread_local_signal;
        // Scheduling Loop.
        // The TERMINATE signal once set, will never be cleared, so memory_order_relaxed synchronization is a sufficient check for this. (I'm pretty sure.)
        while( !(controller_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) & CTL_TERMINATE_EVENT) ){
            std::shared_ptr<server::Session> server_session;
            lk.lock();
            io_mbox_ptr_->sched_signal_cv_ptr->wait(lk, [&]{ 
                return (io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed) || (io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed)!=0)); 
            });
            if(io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)){
                io_mbox_ptr_->msg_flag.store(false, std::memory_order::memory_order_relaxed);
                server_session = io_mbox_ptr_->session;
            }
            thread_local_signal = io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed);
            if(thread_local_signal != 0){
                io_mbox_ptr_->sched_signal_ptr->store(0, std::memory_order::memory_order_relaxed);
            }
            lk.unlock();
            if(thread_local_signal & CTL_TERMINATE_EVENT){
                break;
            }
            if(server_session){
                std::shared_ptr<http::HttpSession> http_session_ptr;
                auto http_client = std::find_if(hcs_.begin(), hcs_.end(), [&](auto& hc){
                    return *hc == server_session;
                });
                if(http_client != hcs_.end()){
                    /* if it is in the http client server list, then we treat this as an incoming response to a client session. */
                    std::shared_ptr<http::HttpClientSession> http_client_ptr = std::static_pointer_cast<http::HttpClientSession>(*http_client);
                    http_client_ptr->read();
                    route_response(http_client_ptr);
                } else {
                    /* Otherwise by default any read request must be a server session. */
                    auto http_session_it = std::find_if(hs_.begin(), hs_.end(), [&](auto& ptr){
                        return *ptr == server_session;
                    });
                    if( http_session_it == hs_.end() ){
                        http_session_ptr = std::make_shared<http::HttpSession>(hs_, server_session);
                        http_session_ptr->read();
                        if(std::get<http::HttpRequest>(*http_session_ptr).verb_started){
                            hs_.push_back(http_session_ptr);
                            route_request(http_session_ptr);
                        }
                    } else {
                        // For our application all session pointers are http session pointers.
                        http_session_ptr = std::static_pointer_cast<http::HttpSession>(*http_session_it);
                        http_session_ptr->read();
                        route_request(http_session_ptr);
                    }
                }
            }
            if (thread_local_signal & CTL_IO_SCHED_END_EVENT){
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
                        // Finish and close all of the HTTP sessions.
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
                        while((*it)->peer_server_sessions().size() > 0){
                            std::shared_ptr<http::HttpSession> next_session = (*it)->peer_server_sessions().back();
                            http::HttpReqRes rr = next_session->get();
                            http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                            res.chunks.push_back(http::HttpChunk{{1}, "]"}); /* Close the JSON stream array. */
                            res.chunks.push_back(http::HttpChunk{{0},""}); /* Close the HTTP stream. */
                            next_session->set(rr);
                            next_session->write([&,next_session](){
                                next_session->close();
                            });
                            (*it)->peer_server_sessions().pop_back();
                        }
                        while((*it)->peer_client_sessions().size() > 0){
                            std::shared_ptr<http::HttpClientSession> next_session = (*it)->peer_client_sessions().back();
                            http::HttpReqRes rr = next_session->get();
                            http::HttpRequest& req = std::get<http::HttpRequest>(rr);
                            req.chunks.push_back(http::HttpChunk{{1},"]"}); /* Close the JSON stream array. */
                            req.chunks.push_back(http::HttpChunk{{0},""}); /* Close the HTTP stream. */
                            next_session->set(rr);
                            next_session->write([&,next_session](){
                                next_session->close();
                            });
                            (*it)->peer_client_sessions().pop_back();
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
                        /* For every peer in the peer table notify them with a state update */
                        // It's only worth constructing the state update values if there are peers to update.
                        if( ((*it)->peer_client_sessions().size() + (*it)->peer_server_sessions().size()) > 0 ){
                            std::shared_ptr<Relation>& finished = (*it)->manifest()[idx];
                            std::string f_key(finished->key());
                            std::string f_val(finished->acquire_value());
                            finished->release_value();
                            boost::json::object jo;
                            jo.emplace(f_key, boost::json::parse(f_val));
                            boost::json::object jf_val;
                            jf_val.emplace("result", jo);
                            std::stringstream ss;
                            ss << jf_val;
                            std::string data(",");
                            data.insert(data.size(), ss.str());
                            for(auto& peer_session: (*it)->peer_client_sessions()){
                                /* Update peers */
                                http::HttpReqRes rr = peer_session->get();
                                http::HttpRequest& req = std::get<http::HttpRequest>(rr);
                                req.chunks.push_back(http::HttpChunk{{data.size()}, data});
                                peer_session->set(rr);
                                peer_session->write([](){return;});
                            }
                            for(auto& peer_session: (*it)->peer_server_sessions()){
                                /* Update peers */
                                http::HttpReqRes rr = peer_session->get();
                                http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                res.chunks.push_back(http::HttpChunk{{data.size()}, data});
                                peer_session->set(rr);
                                peer_session->write([](){return;});
                            }
                        }

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
        pthread_exit(0);
    }

    void Controller::route_response(std::shared_ptr<http::HttpClientSession>& session){
        http::HttpReqRes req_res = session->get();
        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);  
        if(res.next_chunk > 0){
            for(; res.pos < res.next_chunk; ++res.pos){
                std::size_t next_comma = 0;
                const http::HttpChunk& chunk = res.chunks[res.pos];
                const std::size_t chunk_size = chunk.chunk_data.size();
                if(chunk_size == 0){
                    // A 0 length chunk indiciates the end of a session.
                    session->close();
                    return;        
                }
                while(next_comma < chunk_size){
                    std::string_view json_obj_str = find_next_json_object(chunk.chunk_data, next_comma);
                    if(json_obj_str.empty()){
                        continue;
                    } else if (json_obj_str.front() == ']'){
                        /* Function is complete. Terminate the execution context */
                        auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                            auto tmp = std::find(ctx_ptr->peer_client_sessions().begin(), ctx_ptr->peer_client_sessions().end(), session);
                            return (tmp == ctx_ptr->peer_client_sessions().end()) ? false : true;
                        });
                        if(server_ctx != ctx_ptrs.end()){
                            std::size_t num_valid_threads = 0;
                            for(std::size_t i = 0; i < (*server_ctx)->thread_controls().size(); ++i){
                                (*server_ctx)->thread_controls()[i].stop_thread();
                                if((*server_ctx)->thread_controls()[i].is_valid()){
                                    if(num_valid_threads >= 1){
                                        (*server_ctx)->thread_controls()[i].invalidate();
                                    } else {
                                        ++num_valid_threads;
                                    }
                                }
                            }
                            std::string data("{}");
                            for(auto& rel: (*server_ctx)->manifest()){
                                auto& value = rel->acquire_value();
                                if(value.empty()){
                                    value = data;
                                }
                                rel->release_value();
                            }
                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                            io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                        }
                        return;
                    }
                    boost::json::error_code ec;
                    boost::json::value val = boost::json::parse(
                        json_obj_str, 
                        ec
                    );
                    auto ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& cp){
                        auto tmp = std::find(cp->peer_client_sessions().begin(), cp->peer_client_sessions().end(), session);
                        return (tmp == cp->peer_client_sessions().end()) ? false : true;
                    });
                    if(ctx == ctx_ptrs.end()){
                        session->close();
                    } else {
                        if(res.pos == 0){
                            boost::json::array& ja = val.as_object().at("peers").as_array();
                            std::vector<std::string> peers;
                            for(auto& peer: ja){
                                peers.emplace_back(peer.as_string());
                            }
                            std::vector<server::Remote> old_peers = (*ctx)->get_peers();
                            (*ctx)->merge_peer_addresses(peers);
                            std::vector<server::Remote> new_peers = (*ctx)->get_peers();
                            for(auto& peer: new_peers){
                                auto it = std::find_if(old_peers.begin(), old_peers.end(), [&](auto& p){
                                    return (p.ipv4_addr.address.sin_addr.s_addr == peer.ipv4_addr.address.sin_addr.s_addr && p.ipv4_addr.address.sin_port == peer.ipv4_addr.address.sin_port);
                                });
                                if(it == old_peers.end()){
                                    std::shared_ptr<controller::app::ExecutionContext>& ctx_ptr = *ctx;
                                    io_.async_connect(peer, [&, ctx_ptr](const boost::system::error_code& ec, const std::shared_ptr<server::Session>& t_session){
                                        if(!ec){
                                            std::shared_ptr<http::HttpClientSession> client_session = std::make_shared<http::HttpClientSession>(hcs_, t_session);
                                            hcs_.push_back(client_session);
                                            ctx_ptr->peer_client_sessions().push_back(client_session);

                                            boost::json::object jo;
                                            UUID::Uuid uuid = ctx_ptr->execution_context_id();
                                            std::stringstream uuid_str;
                                            uuid_str << uuid;
                                            jo.emplace("uuid", boost::json::string(uuid_str.str()));

                                            std::vector<server::Remote> peers = ctx_ptr->get_peers();
                                            boost::json::array ja;
                                            for(auto& peer: peers){                                                         
                                                ja.push_back(boost::json::string(rtostr(peer)));
                                            }                                                           
                                            jo.emplace("peers", ja);
                                            
                                            boost::json::object jo_ctx;
                                            jo_ctx.emplace("execution_context", jo);
                                            std::stringstream ss;
                                            ss << jo_ctx;
                                            std::string data(ss.str());

                                            std::get<http::HttpRequest>(*client_session) = http::HttpRequest{
                                                http::HttpVerb::PUT,
                                                "/run",
                                                http::HttpVersion::V1_1,
                                                {
                                                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                                                },
                                                {
                                                    {{data.size()}, data}
                                                }
                                            };
                                            client_session->write([&](){ return; });
                                        }
                                    });
                                }
                            }
                        } 
                        boost::json::object& jr = val.as_object().at("result").as_object();
                        for(auto& kvp: jr){
                            std::string k(kvp.key());
                            auto rel = std::find_if((*ctx)->manifest().begin(), (*ctx)->manifest().end(), [&](auto& r){
                                return r->key() == k;
                            });
                            if(rel == (*ctx)->manifest().end()){
                                throw "This shouldn't be possible";
                            }
                            
                            std::string data = boost::json::serialize(kvp.value());
                            (*rel)->acquire_value() = data;
                            (*rel)->release_value();

                            /* Reschedule if necessary */
                            std::ptrdiff_t idx = rel - (*ctx)->manifest().begin();
                            auto& thread = (*ctx)->thread_controls()[idx];     
                            auto execution_idxs = thread.stop_thread();

                            std::size_t manifest_size = (*ctx)->manifest().size();
                            for(auto& i: execution_idxs){
                                // Get the starting relation.
                                std::string start_key((*ctx)->manifest()[i % manifest_size]->key());
                                std::shared_ptr<Relation> start = (*ctx)->manifest().next(start_key, i);
                                if(start->key().size() == 0){
                                    // If the start key is empty, that means that all tasks in the schedule are complete.
                                    // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                                    io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                    io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                                    break;
                                } else {
                                    // Find the index in the manifest of the starting relation.
                                    auto start_it = std::find_if((*ctx)->manifest().begin(), (*ctx)->manifest().end(), [&](auto& rel){
                                        return rel->key() == start->key();
                                    });
                                    if(start_it == (*ctx)->manifest().end()){
                                        throw "This shouldn't be possible";
                                    }
                                    std::ptrdiff_t start_idx = start_it - (*ctx)->manifest().begin();
                                    (*ctx)->thread_controls()[start_idx].notify(i);
                                }                 
                            }
                        }
                    }
                }
            }
            http::HttpReqRes rr = session->get();
            std::get<http::HttpResponse>(rr) = res;
            session->set(rr);
        }
        return;
    }

    void Controller::route_request(std::shared_ptr<http::HttpSession>& session){
        http::HttpReqRes req_res = session->get();
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);            
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

                    if(chunk_size == 0){
                        /* A 0 length chunk represents the end of an HTTP Stream */
                        session->close();
                        return;        
                    }

                    while(next_comma < chunk_size){
                        std::string_view json_obj_str = find_next_json_object(chunk.chunk_data, next_comma);
                        if(json_obj_str.empty()){
                            continue;
                        } else if(json_obj_str.front() == ']'){
                            /* Function is complete. Terminate the execution context */
                            auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                                auto tmp = std::find(ctx_ptr->peer_server_sessions().begin(), ctx_ptr->peer_server_sessions().end(), session);
                                return (tmp == ctx_ptr->peer_server_sessions().end()) ? false : true;
                            });
                            if(server_ctx != ctx_ptrs.end()){
                                std::size_t num_valid_threads = 0;
                                for(std::size_t i = 0; i < (*server_ctx)->thread_controls().size(); ++i){
                                    (*server_ctx)->thread_controls()[i].stop_thread();
                                    if((*server_ctx)->thread_controls()[i].is_valid()){
                                        if(num_valid_threads >= 1){
                                            (*server_ctx)->thread_controls()[i].invalidate();
                                        } else {
                                            ++num_valid_threads;
                                        }
                                    }
                                }
                                std::string data("{}");
                                for (auto& rel: (*server_ctx)->manifest()){
                                    auto& value = rel->acquire_value();
                                    if(value.empty()){
                                        value = data;
                                    }
                                    rel->release_value();
                                }
                                io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                            }
                            return;
                        }

                        boost::json::error_code ec;
                        boost::json::value val = boost::json::parse(
                            json_obj_str,
                            ec
                        );
                        if(ec){throw "Json Parsing failed.";}
                        if(req.route == "/run"){
                            if(req.verb == http::HttpVerb::POST){
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
                                        ctx_ptr->peer_addresses().push_back(io_.local_sctp_address);
                                        ctx_ptrs.push_back(ctx_ptr);
                                        const std::size_t& manifest_size = ctx_ptr->manifest().size();
                                        for( std::size_t i = 0; i < manifest_size; ++i ){
                                            std::thread executor(
                                                [&, ctx_ptr, i](std::shared_ptr<controller::io::MessageBox> mbox_ptr){
                                                    try{
                                                        pthread_t tid = pthread_self();
                                                        ctx_ptr->thread_controls()[i].tid() = tid;
                                                        // The first resume sets up the action runtime environment for execution.
                                                        // The action runtime doesn't have to be set up in a distinct 
                                                        // thread of execution, but since we need to take the time to set up 
                                                        // a thread anyway, deferring the process fork in the execution context until after the 
                                                        // thread is established so that the fork can happen concurrently 
                                                        // is a more performant solution.
                                                        ctx_ptr->thread_controls()[i].resume();
                                                        ctx_ptr->thread_controls()[i].wait();
                                                        ctx_ptr->thread_controls()[i].resume();
                                                        ctx_ptr->thread_controls()[i].signal().fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        mbox_ptr->sched_signal_cv_ptr->notify_all();

                                                    } catch (const boost::context::detail::forced_unwind& e){
                                                        ctx_ptr->thread_controls()[i].signal().fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        mbox_ptr->sched_signal_cv_ptr->notify_all();
                                                    }
                                                },  io_mbox_ptr_
                                            );
                                            executor.detach();
                                        }
                                        /* Initialize the http client sessions */
                                        if(ctx_ptr->execution_context_idx_array().front() == 0){
                                            /* This is the primary context */
                                            // The primary context will have no client peer connections, only server peer connections.
                                            // The primary context must hit the OW API endpoint `concurrency' no. of times with the
                                            // a different execution context idx and the same execution context id each time.
                                        } else {
                                            /* This is a secondary context */
                                            for(auto& peer: ctx_ptr->peer_addresses()){
                                                if(peer.ipv4_addr.address.sin_addr.s_addr != io_.local_sctp_address.ipv4_addr.address.sin_addr.s_addr || peer.ipv4_addr.address.sin_port != io_.local_sctp_address.ipv4_addr.address.sin_port){                                                    
                                                    io_.async_connect(peer, [&, ctx_ptr](const boost::system::error_code& ec, const std::shared_ptr<server::Session>& t_session){
                                                        if(!ec){
                                                            std::shared_ptr<http::HttpClientSession> client_session = std::make_shared<http::HttpClientSession>(hcs_, t_session);
                                                            hcs_.push_back(client_session);
                                                            ctx_ptr->peer_client_sessions().push_back(client_session);
                                                            boost::json::object jo;
                                                            UUID::Uuid uuid = ctx_ptr->execution_context_id();
                                                            std::stringstream uuid_str;
                                                            uuid_str << uuid;
                                                            jo.emplace("uuid", boost::json::string(uuid_str.str()));

                                                            std::vector<server::Remote> peers = ctx_ptr->get_peers();
                                                            boost::json::array ja;
                                                            for(auto& peer: peers){                                                       
                                                                ja.push_back(boost::json::string(rtostr(peer)));
                                                            }                                                           
                                                            jo.emplace("peers", ja);
                                                            
                                                            boost::json::object jo_ctx;
                                                            jo_ctx.emplace("execution_context", jo);
                                                            std::stringstream ss;
                                                            ss << jo_ctx;
                                                            std::string data(ss.str());

                                                            std::get<http::HttpRequest>(*client_session) = http::HttpRequest{
                                                                http::HttpVerb::PUT,
                                                                "/run",
                                                                http::HttpVersion::V1_1,
                                                                {
                                                                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                                                                },
                                                                {
                                                                    {{data.size()}, data}
                                                                }
                                                            };
                                                            client_session->write([&](){ return; });
                                                        }
                                                    });
                                                }
                                            }
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
                                    if (start_it == ctx_ptr->manifest().end()){
                                        // If the start key is past the end of the manifest, that means that
                                        // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                                        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                        io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                                        return;
                                    }
                                    std::ptrdiff_t start_idx = start_it - ctx_ptr->manifest().begin();
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
                            } else if(req.verb == http::HttpVerb::PUT){
                                /* New Connections Go Here. */
                                if(req.pos == 0){
                                    /* A new incoming stream. */
                                    std::string uuid_str(val.as_object().at("execution_context").as_object().at("uuid").as_string());
                                    UUID::Uuid uuid(UUID::Uuid::v4, uuid_str);
                                    auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                                        return (ctx_ptr->execution_context_id() == uuid);
                                    });
                                    if(it != ctx_ptrs.end()){
                                        /* Bind the http session to an existing context. */
                                        (*it)->peer_server_sessions().push_back(session);

                                        //[{"execution_context":{"uuid":"a70ea480860c45e19a5385c68188d1ff","peers":["127.0.0.1:5200"]}} 
                                        /* Merge peers in the peer list with the context peer list. */
                                        boost::json::array& remote_peers = val.as_object().at("execution_context").as_object().at("peers").as_array();
                                        std::vector<std::string> remote_peer_list;
                                        for(auto& rpeer: remote_peers){
                                            remote_peer_list.emplace_back(rpeer.as_string());
                                        }
                                        (*it)->merge_peer_addresses(remote_peer_list);

                                        boost::json::object retjo;
                                        /* Construct a boost json array from the updated peer list */
                                        boost::json::array peers;
                                        for(auto& peer: (*it)->peer_addresses()){
                                           peers.emplace_back(rtostr(peer));
                                        }
                                        retjo.emplace("peers", peers);

                                        /* Construct the results object value */
                                        boost::json::object ro;
                                        for(auto& relation: (*it)->manifest()){
                                            auto& value = relation->acquire_value();
                                            if(!value.empty()){
                                                ro.emplace(relation->key(), boost::json::parse(value));
                                            }
                                            relation->release_value();
                                        }
                                        retjo.emplace("result", ro);

                                        // Prepare data for writing back to the peer.
                                        // The reponse format is:
                                        // [{"peers":["127.0.0.1:5200", "127.0.0.1:5300"], "result":{}}
                                        std::stringstream ss;
                                        ss << retjo;
                                        std::string data("[");
                                        data.insert(data.size(), ss.str());
                                        http::HttpReqRes rr = session->get();
                                        http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                        res = {
                                            http::HttpVersion::V1_1,
                                            http::HttpStatus::CREATED,
                                            {
                                                CONTROLLER_APP_COMMON_HTTP_HEADERS
                                            },
                                            {
                                                {{data.size()}, data}
                                            }
                                        };
                                        session->set(rr);
                                        session->write([](){ return; });
                                    } else {
                                        /* An error condition, no execution context with this ID exists. */
                                        // This condition can occur under at least two different scenarios.
                                        // 1. The action manifest has completed before the initial PUT request for the context
                                        //    arrived from a peer container.
                                        // 2. There was a container failure and restart, with the old container IP address 
                                        //    reassigned by the container orchestration runtime (K8s).

                                        /* In all cases, the only correct response should be to preemptively terminate the stream. */
                                        // Instead of sending a 201 Created response, we send a 404 not found response, with no body.
                                        // The remote peer should respond by truncating the event stream, and cleaning up the execution context.
                                        http::HttpReqRes rr = session->get();
                                        http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                        res = {
                                            http::HttpVersion::V1_1,
                                            http::HttpStatus::NOT_FOUND,
                                            {
                                                {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                                                CONTROLLER_APP_COMMON_HTTP_HEADERS
                                            },
                                            {
                                                {{0}, ""}
                                            }
                                        };
                                        session->set(rr);
                                        session->write([&,session](){
                                            session->close();
                                        });
                                    }
                                } else {
                                    /* An incoming state update. */
                                    /* Search for an execution context that holds the stream. */
                                    auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                                        auto tmp = std::find(ctx_ptr->peer_server_sessions().begin(), ctx_ptr->peer_server_sessions().end(), session);
                                        return (tmp == ctx_ptr->peer_server_sessions().end()) ? false : true;
                                    });
                                    if(server_ctx == ctx_ptrs.end()){
                                        /* There has been an error, the execution context no longer exists. Simply terminate the stream. */
                                        http::HttpReqRes rr = session->get();
                                        http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                        res.chunks.push_back(http::HttpChunk{{0},""});
                                        session->set(rr);
                                        session->write([&,session](){
                                            session->close();
                                        });
                                    } else {
                                        //{"result":{"main":{"msg0":"Hello World!"}}}
                                        /* Extract the results by key, and update the values in the associated relations in the manifest. */
                                        boost::json::object jo = val.as_object().at("result").as_object();
                                        for(auto& kvp: jo){
                                            std::string k(kvp.key());
                                            auto relation = std::find_if((*server_ctx)->manifest().begin(), (*server_ctx)->manifest().end(), [&](auto& r){
                                                return r->key() == k;
                                            });
                                            if(relation == (*server_ctx)->manifest().end()){
                                                throw "This should never happen.";
                                            }
                                            std::string data = boost::json::serialize(kvp.value());
                                            (*relation)->acquire_value() = data;
                                            (*relation)->release_value();

                                            /* Trigger rescheduling if necessary */
                                            std::ptrdiff_t idx = relation - (*server_ctx)->manifest().begin();
                                            auto& thread = (*server_ctx)->thread_controls()[idx];     
                                            auto execution_idxs = thread.stop_thread();

                                            std::size_t manifest_size = (*server_ctx)->manifest().size();
                                            for(auto& i: execution_idxs){
                                                // Get the starting relation.
                                                std::string start_key((*server_ctx)->manifest()[i % manifest_size]->key());
                                                std::shared_ptr<Relation> start = (*server_ctx)->manifest().next(start_key, i);
                                                if(start->key().size() == 0){
                                                    // If the start key is empty, that means that all tasks in the schedule are complete.
                                                    // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                                                    io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                    io_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                                                    break;
                                                } else {
                                                    // Find the index in the manifest of the starting relation.
                                                    auto start_it = std::find_if((*server_ctx)->manifest().begin(), (*server_ctx)->manifest().end(), [&](auto& rel){
                                                        return rel->key() == start->key();
                                                    });
                                                    if(start_it == (*server_ctx)->manifest().end()){
                                                        throw "This shouldn't be possible";
                                                    }
                                                    std::ptrdiff_t start_idx = start_it - (*server_ctx)->manifest().begin();
                                                    (*server_ctx)->thread_controls()[start_idx].notify(i);
                                                }                 
                                            }
                                        }
                                    }
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
                http::HttpReqRes rr = session->get();
                std::get<http::HttpRequest>(rr) = req;
                session->set(rr);
            } else {
                http::HttpReqRes rr;
                http::HttpResponse res = {
                    req.version,
                    http::HttpStatus::NOT_FOUND,
                    {
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        CONTROLLER_APP_COMMON_HTTP_HEADERS
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
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        CONTROLLER_APP_COMMON_HTTP_HEADERS                         
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
                if(std::filesystem::exists(manifest_path)){
                    boost::json::object jrel;
                    for ( auto& relation: ctx.manifest() ){
                        std::string value = relation->acquire_value();
                        relation->release_value();
                        jrel.emplace(relation->key(), boost::json::parse(value));
                    }
                    boost::json::object jres;
                    jres.emplace("result", jrel);

                    UUID::Uuid uuid = ctx.execution_context_id();
                    std::stringstream uuid_str;
                    uuid_str << uuid;
                    jres.emplace("uuid", uuid_str.str());


                    boost::json::object jctx;
                    jctx.emplace("execution_context", jres);

                    std::string data = boost::json::serialize(jctx);
                    std::stringstream len;
                    len << data.size();
                    res = {
                        req.version,
                        http::HttpStatus::OK,
                        {
                            {http::HttpHeaderField::CONTENT_LENGTH, len.str()},
                            CONTROLLER_APP_COMMON_HTTP_HEADERS
                        },
                        {
                            {{data.size()}, data}
                        }
                    };
                } else {
                    auto& relation = ctx.manifest()[0];
                    std::string value = relation->acquire_value();
                    relation->release_value();
                    std::stringstream len;
                    len << value.size();
                    res = {
                        req.version,
                        http::HttpStatus::OK,
                        {
                            {http::HttpHeaderField::CONTENT_LENGTH, len.str()},
                            CONTROLLER_APP_COMMON_HTTP_HEADERS
                        },
                        {
                            {{value.size()}, value}
                        }
                    };
                }
            } else {
                res = {
                    req.version,
                    http::HttpStatus::INTERNAL_SERVER_ERROR,
                    {
                        {http::HttpHeaderField::CONTENT_LENGTH, "0"},
                        CONTROLLER_APP_COMMON_HTTP_HEADERS
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
        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_TERMINATE_EVENT, std::memory_order::memory_order_relaxed);
        io_mbox_ptr_->sched_signal_cv_ptr->notify_all();

        // Wait a reasonable amount of time for the threads to stop gracefully.
        struct timespec ts = {0,50000000};
        nanosleep(&ts, nullptr);
    }

    Controller::~Controller()
    {
        stop();
    }
}// namespace app
}//namespace controller