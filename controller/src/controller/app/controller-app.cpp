#include "controller-app.hpp"
#include "../controller-events.hpp"
#include <application-servers/http/http-session.hpp>
#include "execution-context.hpp"
#include "action-relation.hpp"
#include "../resources/resources.hpp"
#include <charconv>
#include <transport-servers/sctp-server/sctp-session.hpp>

#define CONTROLLER_APP_COMMON_HTTP_HEADERS {http::HttpHeaderField::CONTENT_TYPE, "application/json", "", false, false, false, false, false, false},{http::HttpHeaderField::CONNECTION, "close", "", false, false, false, false, false, false},{http::HttpHeaderField::END_OF_HEADERS, "", "", false, false, false, false, false, false}

namespace controller{
namespace app{
    static std::string_view find_next_json_object(const std::string& data, std::size_t& pos){
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

    static std::string rtostr(const server::Remote& raddr){
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
        io_(io_mbox_ptr_, "/run/controller/controller.sock", ioc),
        ioc_(ioc)
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
        io_(io_mbox_ptr_, upath.string(), ioc, sport),
        ioc_(ioc)
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
        sigset_t sigmask = {};
        int status = sigemptyset(&sigmask);
        if(status == -1){
            std::cerr << "controller-app.cpp:147:sigemptyset failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        status = sigaddset(&sigmask, SIGCHLD);
        if(status == -1){
            std::cerr << "controller-app.cpp:152:sigaddmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        status = sigprocmask(SIG_BLOCK, &sigmask, nullptr);
        if(status == -1){
            std::cerr << "controller-app.cpp:157:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
            throw "what?";
        }
        // Initialize resources I might need.
        errno = 0;
        status = nice(2);
        if(status == -1 && errno != 0){
            std::cerr << "controller-app.cpp:164:nice failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        }
        std::unique_lock<std::mutex> lk(io_mbox_ptr_->mbx_mtx, std::defer_lock);
        std::uint16_t thread_local_signal;
        bool not_timed_out;
        // Scheduling Loop.
        // The TERMINATE signal once set, will never be cleared, so memory_order_relaxed synchronization is a sufficient check for this. (I'm pretty sure.)
        while(true){
            std::shared_ptr<server::Session> server_session;
            lk.lock();
            not_timed_out = io_mbox_ptr_->sched_signal_cv_ptr->wait_for(lk, std::chrono::milliseconds(1000), [&]{ 
                return (io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed) || (io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) & ~CTL_TERMINATE_EVENT)); 
            });
            thread_local_signal = io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed);
            if(thread_local_signal & CTL_TERMINATE_EVENT){
                if(ctx_ptrs.empty()){
                    controller_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                    lk.unlock();
                    break;
                }
            } else if (!not_timed_out){
                lk.unlock();
            } else {
                if(thread_local_signal & ~CTL_TERMINATE_EVENT){
                    io_mbox_ptr_->sched_signal_ptr->fetch_and(~(thread_local_signal & ~CTL_TERMINATE_EVENT), std::memory_order::memory_order_relaxed);
                }
                if(io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)){
                    io_mbox_ptr_->msg_flag.store(false, std::memory_order::memory_order_relaxed);
                    server_session = io_mbox_ptr_->session;
                }
                lk.unlock();
                io_mbox_ptr_->mbx_cv.notify_one();
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
                            // We are not supporting HTTP/1.1 pipelining. So if a client request http chunk is received AFTER the 
                            // server response has been completed, but BEFORE the server response has been received by the client, then
                            // we simply drop the client request http chunk.
                            http::HttpReqRes rr = http_session_ptr->get();
                            http::HttpResponse& server_res = std::get<http::HttpResponse>(rr);
                            auto it = std::find_if(server_res.headers.begin(), server_res.headers.end(), [&](auto& header){
                                return (header.field_name == http::HttpHeaderField::CONTENT_LENGTH);
                            });
                            if((it != server_res.headers.end()) || (server_res.chunks.size() > 0 && server_res.chunks.back().chunk_size != http::HttpBigNum{0})){
                                http_session_ptr->read();
                                route_request(http_session_ptr);
                            }
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
                        auto& ctxp = *it;
                        status = sigprocmask(SIG_BLOCK, &sigmask, nullptr);
                        if(status == -1){
                            std::cerr << "controller-app.cpp:246:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                            throw "what?";
                        }
                        // Check to see if the context is stopped.
                        // std::string __OW_ACTIVATION_ID = (*it)->env()["__OW_ACTIVATION_ID"];
                        // if(!__OW_ACTIVATION_ID.empty()){
                        //     struct timespec ts = {};
                        //     clock_gettime(CLOCK_REALTIME, &ts);
                        //     std::cout << "controller-app.cpp:220:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":__OW_ACTIVATION_ID=" << __OW_ACTIVATION_ID << std::endl;
                        // }
                        if (ctxp->is_stopped()){
                            boost::json::value val;
                            // create the response.
                            http::HttpReqRes rr;
                            std::get<http::HttpResponse>(rr) = create_response(*ctxp);
                            while(ctxp->sessions().size() > 0 ){
                                std::shared_ptr<http::HttpSession> next_session = ctxp->sessions().back();
                                next_session->set(rr);
                                next_session->write(
                                    [&, next_session](){
                                        next_session->close();
                                    }
                                );
                                ctxp->sessions().pop_back();
                                flush_wsk_logs();
                            }
                            // Finish and close all of the HTTP sessions.
                            while(ctxp->peer_server_sessions().size() > 0){
                                std::shared_ptr<http::HttpSession> next_session = ctxp->peer_server_sessions().back();
                                http::HttpReqRes rr = next_session->get();
                                http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                http::HttpChunk new_chunk = {};
                                new_chunk.chunk_size = {1};
                                new_chunk.chunk_data = "]";
                                res.chunks.push_back(new_chunk); /* Close the JSON stream array. */
                                new_chunk.chunk_size = {0};
                                new_chunk.chunk_data.clear();
                                res.chunks.push_back(new_chunk); /* Close the HTTP stream. */
                                next_session->set(rr);
                                next_session->write([&,next_session](){
                                    next_session->close();
                                });
                                ctxp->peer_server_sessions().pop_back();
                            }
                            while(ctxp->peer_client_sessions().size() > 0){
                                std::shared_ptr<http::HttpClientSession> next_session = ctxp->peer_client_sessions().back();
                                http::HttpReqRes rr = next_session->get();
                                http::HttpRequest& req = std::get<http::HttpRequest>(rr);
                                http::HttpChunk new_chunk = {};
                                new_chunk.chunk_size = {1};
                                new_chunk.chunk_data = "]";
                                req.chunks.push_back(new_chunk); /* Close the JSON stream array. */
                                new_chunk.chunk_size = {0};
                                new_chunk.chunk_data.clear();
                                req.chunks.push_back(new_chunk); /* Close the HTTP stream. */
                                next_session->set(rr);
                                next_session->write([&,next_session](){
                                    return;
                                });
                                ctxp->peer_client_sessions().pop_back();
                            }
                            ctx_ptrs.erase(it); // This invalidates the iterator in the loop, so we have to perform the original search again.
                            // Find a context that has a stopped thread.
                            it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& ctx_ptr){
                                auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](auto& thread){
                                    return thread.is_stopped() && thread.is_valid();
                                });
                                return (tmp == ctx_ptr->thread_controls().end()) ? false : true;
                            });
                            if(sigprocmask(SIG_UNBLOCK, &sigmask, nullptr) == -1){
                                std::cerr << "controller-app.cpp:306:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                throw "what?";
                            }
                        } else {
                            // Evaluate which thread to execute next and notify it.
                            auto stopped_thread = std::find_if((*it)->thread_controls().begin(), (*it)->thread_controls().end(), [&](auto& thread){
                                return thread.is_stopped() && thread.is_valid();
                            });
                            // invalidate the thread.
                            std::vector<std::size_t> execution_context_idxs = stopped_thread->invalidate();
                            // Do the subsequent steps only if there are execution idxs that exited normally.
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
                                boost::json::error_code ec;
                                boost::json::value jv = boost::json::parse(f_val, ec);
                                if(ec){
                                    std::cerr << "controller-app.cpp:331:JSON parsing failed:" << ec.message() << ":value:" << f_val << std::endl;
                                    throw "This shouldn't happen.";
                                }
                                jo.emplace(f_key, jv);
                                boost::json::object jf_val;
                                jf_val.emplace("result", jo);
                                std::string data(",");
                                std::string jsonf_val = boost::json::serialize(jf_val);
                                data.append(jsonf_val);
                                for(auto& peer_session: (*it)->peer_client_sessions()){
                                    /* Update peers */
                                    http::HttpReqRes rr = peer_session->get();
                                    http::HttpRequest& req = std::get<http::HttpRequest>(rr);
                                    http::HttpChunk chunk = {};
                                    chunk.chunk_size = {data.size()};
                                    chunk.chunk_data = data;
                                    req.chunks.push_back(chunk);
                                    peer_session->set(rr);
                                    peer_session->write([&, peer_session](){return;});
                                }
                                for(auto& peer_session: (*it)->peer_server_sessions()){
                                    /* Update peers */
                                    http::HttpReqRes rr = peer_session->get();
                                    http::HttpResponse& res = std::get<http::HttpResponse>(rr);
                                    http::HttpChunk chunk = {};
                                    chunk.chunk_size = {data.size()};
                                    chunk.chunk_data = data;
                                    res.chunks.push_back(chunk);
                                    peer_session->set(rr);
                                    peer_session->write([&, peer_session](){return;});
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
        }
        return;
    }

    void Controller::route_response(std::shared_ptr<http::HttpClientSession>& session){
        http::HttpReqRes req_res = session->get();
        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);
        auto it = std::find_if(req.headers.begin(), req.headers.end(), [&](auto& header){
            return (header.field_name == http::HttpHeaderField::CONTENT_LENGTH);
        });
        if(res.status == http::HttpStatus::CREATED){
            for(; res.pos < res.next_chunk; ++res.pos){
                std::size_t next_comma = 0;
                const http::HttpChunk& chunk = res.chunks[res.pos];
                const http::HttpBigNum& chunk_size = chunk.chunk_size;
                if(chunk_size == http::HttpBigNum{0}){
                    // A 0 length chunk indiciates the end of a session.
                    session->close();
                    return;
                }
                while(http::HttpBigNum{next_comma} < chunk_size){
                    std::string_view json_obj_str = find_next_json_object(chunk.chunk_data, next_comma);
                    if(json_obj_str.empty()){
                        std::cerr << "controller-app.cpp:407:json_obj_str is empty" << std::endl;
                        continue;
                    } else if (json_obj_str.front() == ']'){
                        /* Function is complete. Terminate the execution context */
                        auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                            auto tmp = std::find(ctx_ptr->peer_client_sessions().begin(), ctx_ptr->peer_client_sessions().end(), session);
                            return (tmp == ctx_ptr->peer_client_sessions().end()) ? false : true;
                        });
                        if(server_ctx != ctx_ptrs.end()){
                            std::size_t num_valid_threads = 0;
                            for(auto& thread_control: (*server_ctx)->thread_controls()){
                                thread_control.stop_thread();
                                if(thread_control.is_valid()){
                                    if(num_valid_threads > 0){
                                        thread_control.invalidate();
                                    } else {
                                        ++num_valid_threads;
                                    }
                                }
                            }
                            for(auto& rel: (*server_ctx)->manifest()){
                                auto& value = rel->acquire_value();
                                if(value.empty()){
                                    value = "{}";
                                }
                                rel->release_value();
                            }
                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                            // We do not support HTTP/1.1 pipelining so the client session can only be closed after the ENTIRE server response
                            // has been consumed.
                            break;
                        } else {
                            // We do not support HTTP/1.1 pipelining, so the HTTP client session can only be closed after the ENTIRE 
                            // server response has been consumed.
                            break;
                        }
                    } else if((it != req.headers.end()) || (req.chunks.size() > 0 && req.chunks.back().chunk_size == http::HttpBigNum{0})){
                        // We do not support HTTP/1.1 pipelining so we will only close the HTTP client stream after the ENTIRE server response has
                        // been consumed.
                        break;
                    }
                    boost::json::error_code ec;
                    boost::json::value val = boost::json::parse(
                        json_obj_str, 
                        ec
                    );
                    if(ec){
                        std::cerr << "controller-app.cpp:455:JSON Parsing failed:" << ec.message() <<":value:" << json_obj_str << std::endl;
                        throw "this shouldn't happen.";
                    }
                    auto ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& cp){
                        auto tmp = std::find(cp->peer_client_sessions().begin(), cp->peer_client_sessions().end(), session);
                        return (tmp == cp->peer_client_sessions().end()) ? false : true;
                    });
                    if(ctx == ctx_ptrs.end()){
                        session->close();
                        return;
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
                                            std::string data("[");
                                            std::string json_str(boost::json::serialize(jo_ctx));
                                            data.append(json_str);
                                            http::HttpRequest nreq = {};
                                            nreq.verb = http::HttpVerb::PUT;
                                            nreq.route = "/run";
                                            nreq.version = http::HttpVersion::V1_1;
                                            nreq.headers = {
                                                CONTROLLER_APP_COMMON_HTTP_HEADERS
                                            };
                                            http::HttpChunk nchunk = {};
                                            nchunk.chunk_size = {data.size()};
                                            nchunk.chunk_data = data;
                                            nreq.chunks = {
                                                nchunk
                                            };
                                            std::get<http::HttpRequest>(*client_session) = nreq;
                                            client_session->write([&, client_session](){ return; });
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
                                std::cerr << "No relation with this key could be found in the manifest." << std::endl;
                                throw "This shouldn't be possible";
                            }
                            
                            std::string data = boost::json::serialize(kvp.value());
                            auto& value = (*rel)->acquire_value();
                            if(value.empty()){
                                value = data;
                            }
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
                                if(start->key().empty()){
                                    // If the start key is empty, that means that all tasks in the schedule are complete.
                                    std::size_t num_valid_threads = 0;
                                    for(auto& thread_control: (*ctx)->thread_controls()){
                                        thread_control.stop_thread();
                                        if(thread_control.is_valid()){
                                            if(num_valid_threads > 0){
                                                thread_control.invalidate();
                                            } else {
                                                ++num_valid_threads;
                                            }
                                        }
                                    }
                                    for(auto& rel: (*ctx)->manifest()){
                                        auto& value = rel->acquire_value();
                                        if(value.empty()){
                                            value = "{}";
                                        }
                                        rel->release_value();
                                    }
                                    io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                    io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                    break;
                                } else {
                                    thread.invalidate();
                                    // Find the index in the manifest of the starting relation.
                                    auto start_it = std::find_if((*ctx)->manifest().begin(), (*ctx)->manifest().end(), [&](auto& rel){
                                        return rel->key() == start->key();
                                    });
                                    if(start_it == (*ctx)->manifest().end()){
                                        std::cerr << "Relation does not exist in the active manifest." << std::endl;
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
        } else {
            auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                auto tmp = std::find(ctx_ptr->peer_client_sessions().begin(), ctx_ptr->peer_client_sessions().end(), session);
                return (tmp == ctx_ptr->peer_client_sessions().end()) ? false : true;
            });
            if(server_ctx == ctx_ptrs.end()){
                // The doesn't belong to a context so just close it.
                session->close();
            } else {
                auto& ctx_ptr = *server_ctx;
                std::vector<server::Remote> peers = ctx_ptr->get_peers();
                if(peers.size() < 3){
                    // A peer list size of 1 indicates that the only peer is myself.
                    // A peer list size of 2 indicates that the only peers I have are myself, and a primary context.
                    // If the only peers I have in my list are myself and a primary context AND an error code is returned by a peer (MUST be the primary context).
                    // Then the execution context is finished and I should terminate the context.
                    std::size_t num_valid_threads = 0;
                    for(std::size_t i=0; i < ctx_ptr->thread_controls().size(); ++i){
                        auto& thread = ctx_ptr->thread_controls()[i];
                        auto& relation = ctx_ptr->manifest()[i];
                        auto& value = relation->acquire_value();
                        if(value.empty()){
                            value = "{}";
                        }
                        relation->release_value();
                        thread.stop_thread();
                        if(thread.is_valid()){
                            if(num_valid_threads > 0){
                                thread.invalidate();
                            } else {
                                ++num_valid_threads;
                            }
                        }
                    }
                    io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                    io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                } else {
                    // A peer list of >= 3 AND an error code in the response message indiciates that 
                    // one of the peers that ARE NOT the primary context has a failure (or perhaps a race condition).
                    // The correct behaviour here is to close the session and not retry.
                    session->close();
                }
            }
        }
        return;
    }

    void Controller::route_request(std::shared_ptr<http::HttpSession>& session){
        http::HttpReqRes req_res = session->get();
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);            
        // Start processing chunks, iff next_chunk > 0.
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
                const http::HttpBigNum& chunk_size = chunk.chunk_size;

                if(chunk_size == http::HttpBigNum{0}){
                    // A 0 length chunk means that the entire HTTP Request has been consumed.
                    // We have to maintain the session until the entire HTTP response has been completely processed.
                    return;
                }

                while(http::HttpBigNum{next_comma} < chunk_size){
                    std::string_view json_obj_str = find_next_json_object(chunk.chunk_data, next_comma);
                    if(json_obj_str.empty()){
                        std::cerr << "controller-app.cpp:675:json_obj_str is empty" << std::endl;
                        continue;
                    } else if(json_obj_str.front() == ']'){
                        /* Function is complete. Terminate the execution context */
                        auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                            auto tmp = std::find(ctx_ptr->peer_server_sessions().begin(), ctx_ptr->peer_server_sessions().end(), session);
                            return (tmp == ctx_ptr->peer_server_sessions().end()) ? false : true;
                        });
                        if(server_ctx != ctx_ptrs.end()){
                            std::size_t num_valid_threads = 0;
                            for(auto& thread_control: (*server_ctx)->thread_controls()){
                                thread_control.stop_thread();
                                if(thread_control.is_valid()){
                                    if(num_valid_threads > 0){
                                        thread_control.invalidate();
                                    } else {
                                        ++num_valid_threads;
                                    }
                                }
                            }
                            for (auto& rel: (*server_ctx)->manifest()){
                                auto& value = rel->acquire_value();
                                if(value.empty()){
                                    value = "{}";
                                }
                                rel->release_value();
                            } 
                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                            break;
                        } else {
                            break;
                        }
                    }

                    boost::json::error_code ec;
                    boost::json::value val = boost::json::parse(
                        json_obj_str,
                        ec
                    );
                    if(ec){
                        std::cerr << "controller-app.cpp:716:JSON parsing failed:" << ec.message() << ":value:" << json_obj_str << std::endl;
                        throw "Json Parsing failed.";
                    }
                    if(req.route == "/run"){
                        if(req.verb == http::HttpVerb::POST){
                            controller::resources::run::Request run(val.as_object());
                            auto env = run.env();
                            // std::string __OW_ACTIVATION_ID = env["__OW_ACTIVATION_ID"];
                            // if(!__OW_ACTIVATION_ID.empty()){
                            //     struct timespec ts = {};
                            //     int status = clock_gettime(CLOCK_REALTIME, &ts);
                            //     if(status != -1){
                            //         std::cout << "controller-app.cpp:728:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":__OW_ACTIVATION_ID=" << __OW_ACTIVATION_ID << std::endl;
                            //     }
                            // }
                            // Create a fiber continuation for processing the request.
                            std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::run::handle(run, ctx_ptrs); 
                            auto http_it = std::find(ctx_ptr->sessions().cbegin(), ctx_ptr->sessions().cend(), session);
                            if(http_it == ctx_ptr->sessions().cend()){
                                ctx_ptr->sessions().push_back(session);
                            }
                            ctx_ptr->env() = env;
                            if (initialized_){
                                // Initialize threads only once.
                                // If ctx_ptr is already in the controller ctx_ptrs then threads don't need to be initialized again.
                                auto ctx_it = std::find(ctx_ptrs.begin(), ctx_ptrs.end(), ctx_ptr);
                                if (ctx_it == ctx_ptrs.end()){
                                    auto it = std::find_if(ctx_ptr->peer_addresses().begin(), ctx_ptr->peer_addresses().end(), [&](auto& peer){
                                        return (peer.ipv4_addr.address.sin_addr.s_addr == io_.local_sctp_address.ipv4_addr.address.sin_addr.s_addr && peer.ipv4_addr.address.sin_port == io_.local_sctp_address.ipv4_addr.address.sin_port);
                                    });
                                    if(it == ctx_ptr->peer_addresses().end()){
                                        ctx_ptr->peer_addresses().push_back(io_.local_sctp_address);
                                    }
                                    ctx_ptrs.push_back(ctx_ptr);
                                    #ifndef DEBUG
                                    if(val.get_object().at("value").as_object().contains("execution_context")){
                                        // If this is a new execution_context AND the function input parameters
                                        // are wrapped in an existing execution context AND the primary peer address in the 
                                        // incoming execution context is equal to the local SCTP address; then this execution context has already
                                        // completed. The correct behaviour here is to terminate the context.

                                        // We don't want to execute this logic in debug builds as it guards against explicitly setting 
                                        // the UUID for new execution contexts (new execution contexts MUST use a locally generated UUID).
                                        boost::json::array& ja = val.get_object()["value"].get_object()["execution_context"].at("peers").as_array();
                                        std::string p(ja[0].as_string());
                                        std::size_t pos = p.find(':');
                                        std::string_view pip(&p[0],pos);
                                        std::string_view pport(&p[pos+1], p.size()-pos-1);
                                        std::uint16_t portnum = 0;
                                        std::from_chars_result fcres = std::from_chars(pport.data(), pport.data()+pport.size(), portnum,10);
                                        if(fcres.ec != std::errc()){
                                            std::cerr << "Converting: " << pport << " to uint16_t failed: " << std::make_error_code(fcres.ec).message() << std::endl;
                                            throw "This shouldn't happen!";
                                        }
                                        struct sockaddr_in rip = {};
                                        rip.sin_family = AF_INET;
                                        rip.sin_port = htons(portnum);
                                        std::string pip_str(pip);
                                        int ec = inet_aton(pip_str.c_str(), &rip.sin_addr);
                                        if(ec == 0){
                                            std::cerr << "Converting: " << pip << " to struct in_addr failed." << std::endl;
                                            throw "This shouldn't happen!";
                                        }
                                        if(rip.sin_addr.s_addr == io_.local_sctp_address.ipv4_addr.address.sin_addr.s_addr && rip.sin_port == io_.local_sctp_address.ipv4_addr.address.sin_port){
                                            std::size_t num_valid_threads = 0;
                                            for(std::size_t i=0; i < ctx_ptr->thread_controls().size(); ++i){
                                                auto& thread = ctx_ptr->thread_controls()[i];
                                                auto& relation = ctx_ptr->manifest()[i];
                                                relation->acquire_value() = "{}";
                                                relation->release_value();
                                                thread.signal().store(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                if(num_valid_threads > 0){
                                                    thread.invalidate();
                                                } else {
                                                    ++num_valid_threads;
                                                }
                                            }
                                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                            return;                   
                                        }
                                    }
                                    #endif
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
                                                    mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                } catch (const boost::context::detail::forced_unwind& e){
                                                    ctx_ptr->thread_controls()[i].signal().fetch_or(CTL_IO_SCHED_END_EVENT | CTL_IO_SCHED_START_EVENT, std::memory_order::memory_order_relaxed);                             
                                                    mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT | CTL_IO_SCHED_START_EVENT, std::memory_order::memory_order_relaxed);
                                                    mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                }
                                            },  io_mbox_ptr_
                                        );
                                        executor.detach();
                                    }
                                    ctx_ptr->wait_for_sync();
                                    /* Initialize the http client sessions */
                                    if(ctx_ptr->execution_context_idx_array().front() == 0){
                                        /* This is the primary context */
                                        // The primary context will have no client peer connections, only server peer connections.
                                        // The primary context must hit the OW API endpoint `concurrency' no. of times with the
                                        // a different execution context idx and the same execution context id each time.
                                        std::size_t concurrency = ctx_ptr->manifest().concurrency();
                                        if(concurrency > 1){
                                            char* __OW_ACTION_NAME = getenv("__OW_ACTION_NAME");
                                            char* __OW_API_HOST = getenv("__OW_API_HOST");
                                            if(__OW_ACTION_NAME == nullptr){
                                                std::cerr << "__OW_ACTION_NAME envvar is not set!" << std::endl;
                                                throw "This shouldn't happen.";
                                            } else if (__OW_API_HOST == nullptr){
                                                std::cerr << "__OW_API_HOST envvar is not set!" << std::endl;
                                                throw "This shouldn't happen.";
                                            }

                                            std::string __OW_API_KEY = env["__OW_API_KEY"];
                                            if(__OW_API_KEY.empty()){
                                                std::cerr << "__OW_API_KEY envvar is not set!" << std::endl;
                                                throw "This shouldn't happen.";
                                            }

                                            // Wrap the json parameter value val in an execution context.
                                            std::stringstream uuid;
                                            uuid << ctx_ptr->execution_context_id();
                                            boost::json::object jo;
                                            jo.emplace("uuid", boost::json::string(uuid.str()));
                                            std::vector<server::Remote> peers = ctx_ptr->peer_addresses();
                                            boost::json::array ja;
                                            for(auto& peer: peers){
                                                ja.push_back(boost::json::string(rtostr(peer)));
                                            }
                                            jo.emplace("peers", ja);
                                            jo.emplace("value", val.at("value").as_object());

                                            // Construct the curl command.
                                            const char* bin_curl = "/usr/bin/curl";
                                            std::vector<const char*> argv;
                                            argv.reserve(9*concurrency+2);
                                            argv.push_back(bin_curl);
                                            argv.push_back("--parallel-immediate");
                                            argv.push_back("-Z");
                                            argv.push_back("--no-progress-meter");

                                            /*For each other concurrent invocation, hit the __OW_API_HOST actions endpoint at
                                            $__OW_API_HOST/api/v1/$__OW_ACTION_NAME. With basic http authentication -u "$__OW_API_KEY".
                                            */
                                            // The arguments required for each request before passing cURL the --next flag.
                                            // HTTP basic authentication
                                            argv.push_back("-u");
                                            argv.push_back(__OW_API_KEY.c_str());
                                            std::vector<std::string> data_vec;
                                            data_vec.reserve(concurrency);

                                            // Compute the index for each subsequent context by partitioning the manifest.
                                            std::size_t manifest_size = ctx_ptr->manifest().size();
                                            std::vector<std::size_t> indices;
                                            indices.reserve(concurrency);
                                            if(manifest_size < concurrency){
                                                for (std::size_t i = 0; i < concurrency; ++i){
                                                    indices.push_back(i);
                                                }
                                            } else {
                                                for (std::size_t i = 0; i < concurrency; ++i){
                                                    indices.push_back(i*(manifest_size/concurrency));
                                                }
                                            }
                                            // Emplace the first context index.
                                            jo.emplace("idx", indices[1]);
                                            boost::json::object jctx;
                                            jctx.emplace("execution_context", jo);
                                            data_vec.emplace_back(boost::json::serialize(jctx));

                                            // provide the data for the first request.
                                            argv.push_back("--json");
                                            argv.push_back(data_vec.back().c_str());

                                            // Fully qualified action names are given in terms of a filesystem path.
                                            // of the form /{NAMESPACE}/{PACKAGE}/{ACTION_NAME}
                                            std::filesystem::path action_name(__OW_ACTION_NAME);
                                            std::string url(__OW_API_HOST);
                                            url.append("/api/v1/namespaces/");
                                            url.append(action_name.relative_path().begin()->string());
                                            url.append("/actions/");
                                            url.append(action_name.filename().string());
                                            argv.push_back(url.c_str());
                                            argv.push_back("-o");
                                            argv.push_back("/dev/null");

                                            for(std::size_t i = 2; i < concurrency; ++i){
                                                /* For every subsequent request add --next and repeat the data with a modified index. */
                                                argv.push_back("--next");
                                                argv.push_back("--no-progress-meter");
                                                argv.push_back("-u");
                                                argv.push_back(__OW_API_KEY.c_str());
                                                argv.push_back("--json");
                                                jctx["execution_context"].get_object()["idx"] = indices[i];
                                                data_vec.emplace_back(boost::json::serialize(jctx));
                                                argv.push_back(data_vec.back().c_str());
                                                argv.push_back(url.c_str());
                                                argv.push_back("-o");
                                                argv.push_back("/dev/null");
                                            }
                                            argv.push_back(nullptr);

                                            std::size_t max_retries = 5;
                                            std::size_t counter = 0;
                                            while(counter < max_retries){
                                                pid_t pid = fork();
                                                switch(pid)
                                                {
                                                    case 0:
                                                    {
                                                        sigset_t sigmask = {};
                                                        int status = sigemptyset(&sigmask);
                                                        if(status == -1){
                                                            std::cerr << "controller-app.cpp:947:sigemptyset failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                            throw "what?";
                                                        }
                                                        status = sigaddset(&sigmask, SIGCHLD);
                                                        if(status == -1){
                                                            std::cerr << "controller-app.cpp:952:sigaddmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                            throw "what?";
                                                        }
                                                        status = sigprocmask(SIG_BLOCK, &sigmask, nullptr);
                                                        if(status == -1){
                                                            std::cerr << "controller-app.cpp:957:sigprocmask failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                            throw "what?";
                                                        }
                                                        execve(bin_curl, const_cast<char* const*>(argv.data()), environ);
                                                        exit(1);
                                                        break;
                                                    }
                                                    case -1:
                                                    {
                                                        switch(errno)
                                                        {
                                                            case EAGAIN:
                                                            {
                                                                ++counter;
                                                                if(counter >= max_retries){
                                                                    std::cerr << "controller-app.cpp:972:fork cURL failed:" << std::make_error_code(std::errc(errno)).message() << ":GIVING UP" << std::endl;
                                                                    raise(SIGTERM);
                                                                    break;
                                                                }
                                                                struct timespec ts = {0,5000000};
                                                                nanosleep(&ts, nullptr);
                                                                std::cerr << "controller-app.cpp:978:fork cURL failed:" << std::make_error_code(std::errc(errno)).message() << ":RETRYING" << std::endl;
                                                                break;
                                                            }
                                                            default:
                                                                std::cerr << "Fork cURL failed: " << std::make_error_code(std::errc(errno)).message() << std::endl;
                                                                throw "This shouldn't happen!";      
                                                        }
                                                        break;
                                                    }
                                                    default:
                                                        counter = max_retries;
                                                        break;
                                                }
                                            }
                                        }
                                        // waitpid is handled by trapping SIGCHLD.
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
                                                        std::string data("[");
                                                        std::string json_str(boost::json::serialize(jo_ctx));
                                                        data.append(json_str);
                                                        http::HttpRequest nreq = {};
                                                        nreq.verb = http::HttpVerb::PUT;
                                                        nreq.route = "/run";
                                                        nreq.version = http::HttpVersion::V1_1;
                                                        nreq.headers = {
                                                            CONTROLLER_APP_COMMON_HTTP_HEADERS
                                                        };
                                                        http::HttpChunk nc = {};
                                                        nc.chunk_size = {data.size()};
                                                        nc.chunk_data = data;
                                                        nreq.chunks = {
                                                            nc
                                                        };
                                                        std::get<http::HttpRequest>(*client_session) = nreq;
                                                        client_session->write([&, client_session](){ return; });
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
                                    std::cerr << "controller-app.cpp:1051:there are no matches for rel->key() == start->key():start->key()=" << start->key() << std::endl;
                                    // If the start key is past the end of the manifest, that means that
                                    // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                                    io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                    io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                    return;
                                }
                                std::ptrdiff_t start_idx = start_it - ctx_ptr->manifest().begin();
                                ctx_ptr->thread_controls()[start_idx].notify(execution_idx);
                            } else {
                                // invalidate the fibers.
                                std::cerr << "controller-app.cpp:1062:/run route reached before initialization." << std::endl;
                                http::HttpReqRes rr;
                                while(ctx_ptr->sessions().size() > 0)
                                {
                                    std::shared_ptr<http::HttpSession>& next_session = ctx_ptr->sessions().back();
                                    std::get<http::HttpResponse>(rr) = create_response(*ctx_ptr);
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
                                    const boost::json::array& remote_peers = val.as_object().at("execution_context").as_object().at("peers").as_array();
                                    std::vector<std::string> remote_peer_list;
                                    for(const auto& rpeer: remote_peers){
                                        remote_peer_list.emplace_back(rpeer.as_string());
                                    }
                                    (*it)->merge_peer_addresses(remote_peer_list);

                                    boost::json::object retjo;
                                    /* Construct a boost json array from the updated peer list */
                                    boost::json::array peers;
                                    for(const auto& peer: (*it)->peer_addresses()){
                                        peers.emplace_back(rtostr(peer));
                                    }
                                    retjo.emplace("peers", peers);

                                    /* Construct the results object value */
                                    boost::json::object ro;
                                    for(auto& relation: (*it)->manifest()){
                                        const auto& value = relation->acquire_value();
                                        if(!value.empty()){
                                            boost::json::error_code ec;
                                            boost::json::value jv = boost::json::parse(value, ec);
                                            if(ec){
                                                std::cerr << "controller-app.cpp:1115:JSON parsing failed:" << ec.message() << ":value:" << value << std::endl;
                                                throw "This shouldn't be possible.";
                                            }
                                            ro.emplace(relation->key(), jv);
                                        }
                                        relation->release_value();
                                    }
                                    retjo.emplace("result", ro);

                                    // Prepare data for writing back to the peer.
                                    // The reponse format is:
                                    // [{"peers":["127.0.0.1:5200", "127.0.0.1:5300"], "result":{}}
                                    std::string data("[");
                                    std::string json_str(boost::json::serialize(retjo));
                                    data.append(json_str);
                                    http::HttpReqRes rr = session->get();
                                    http::HttpResponse nres = {};
                                    nres.version = http::HttpVersion::V1_1;
                                    nres.status = http::HttpStatus::CREATED;
                                    nres.headers = {
                                        CONTROLLER_APP_COMMON_HTTP_HEADERS
                                    };
                                    http::HttpChunk nc = {};
                                    nc.chunk_size = {data.size()};
                                    nc.chunk_data = data;
                                    nres.chunks = {
                                        nc
                                    };
                                    std::get<http::HttpResponse>(rr) = nres;
                                    session->set(rr);
                                    session->write([&, session](){ return; });
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
                                    http::HttpResponse nres = {};
                                    nres.version = http::HttpVersion::V1_1;
                                    nres.status = http::HttpStatus::NOT_FOUND;
                                    http::HttpHeader content_length = {};
                                    content_length.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                                    content_length.field_value = "0";
                                    nres.headers = {
                                        content_length,
                                        CONTROLLER_APP_COMMON_HTTP_HEADERS
                                    };
                                    nres.chunks = {
                                        {}
                                    };
                                    std::get<http::HttpResponse>(rr) = nres;
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
                                    http::HttpChunk nc = {};
                                    nc.chunk_size = {1};
                                    nc.chunk_data = "]";
                                    res.chunks.push_back(nc);
                                    nc.chunk_size = {0};
                                    nc.chunk_data.clear();
                                    res.chunks.push_back(nc);
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
                                            std::cerr << "Relation does not exist in the active manifest." << std::endl;
                                            throw "This should never happen.";
                                        }
                                        std::string data = boost::json::serialize(kvp.value());
                                        auto& value = (*relation)->acquire_value();
                                        if(value.empty()){
                                            value = data;
                                        }
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
                                            if(start->key().empty()){
                                                // If the start key is empty, that means that all tasks in the schedule are complete.
                                                std::size_t num_valid_threads = 0;                                                    
                                                for(auto& thread_control: (*server_ctx)->thread_controls()){
                                                    thread_control.stop_thread();
                                                    if(thread_control.is_valid()){
                                                        if(num_valid_threads > 0){
                                                            thread_control.invalidate();
                                                        } else {
                                                            ++num_valid_threads;
                                                        }
                                                    }
                                                }
                                                for(auto& rel: (*server_ctx)->manifest()){
                                                    auto& value = rel->acquire_value();
                                                    if(value.empty()){
                                                        value = "{}";
                                                    }
                                                    rel->release_value();
                                                }
                                                io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                                break;
                                            } else {
                                                thread.invalidate();
                                                // Find the index in the manifest of the starting relation.
                                                auto start_it = std::find_if((*server_ctx)->manifest().begin(), (*server_ctx)->manifest().end(), [&](auto& rel){
                                                    return rel->key() == start->key();
                                                });
                                                if(start_it == (*server_ctx)->manifest().end()){
                                                    std::cerr << "Relation does not exist in the active manifest." << std::endl;
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
                        res = create_response(*ctx_ptr);
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
            http::HttpResponse res = {};
            res.version = req.version;
            res.status = http::HttpStatus::NOT_FOUND;
            http::HttpHeader content_length = {};
            content_length.field_name = http::HttpHeaderField::CONTENT_LENGTH;
            content_length.field_value = "0";
            res.headers = {
                content_length,
                CONTROLLER_APP_COMMON_HTTP_HEADERS
            };
            res.chunks = {
                {}
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

    http::HttpResponse Controller::create_response(ExecutionContext& ctx){
        http::HttpResponse res = {};
        if(ctx.route() == controller::resources::Routes::RUN){
            // The run route is not necessarily single threaded so we must use threadsafe get.
            std::shared_ptr<http::HttpSession> session = ctx.sessions().back();
            http::HttpReqRes rr = session->get();
            http::HttpRequest& req = std::get<http::HttpRequest>(rr);
            if(!initialized_){
                res.version = req.version;
                res.status = http::HttpStatus::NOT_FOUND;
                http::HttpHeader content_length = {};
                content_length.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                content_length.field_value = "0";
                res.headers = {
                    content_length,
                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                };
                res.chunks = {
                    {}
                };
            } else if (ctx.is_stopped()){
                boost::json::object jv;
                const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
                if ( __OW_ACTIONS == nullptr ){
                    std::cerr << "Environment Variable __OW_ACTIONS is not defined." << std::endl;
                    throw "environment variable __OW_ACTIONS is not defined.";
                }
                std::filesystem::path path(__OW_ACTIONS);
                std::filesystem::path manifest_path(path/"action-manifest.json");
                if(std::filesystem::exists(manifest_path)){
                    boost::json::object jrel;
                    for ( auto& relation: ctx.manifest() ){
                        std::string value = relation->acquire_value();
                        relation->release_value();
                        boost::json::error_code ec;
                        boost::json::value jv = boost::json::parse(value, ec);
                        if(ec){
                            std::cerr << "controller-app.cpp:1360:JSON parsing failed:" << ec.message() << ":value:" << value << std::endl;
                            throw "This shouldn't happen.";
                        }
                        jrel.emplace(relation->key(), jv);
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
                    res.version = req.version;
                    res.status = http::HttpStatus::OK;
                    http::HttpHeader content_length = {};
                    content_length.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                    content_length.field_value = len.str();
                    res.headers = {
                        content_length,
                        CONTROLLER_APP_COMMON_HTTP_HEADERS
                    };
                    http::HttpChunk nc = {};
                    nc.chunk_size = {data.size()};
                    nc.chunk_data = data;
                    res.chunks = {
                        nc
                    };
                } else {
                    auto& relation = ctx.manifest()[0];
                    std::string value = relation->acquire_value();
                    relation->release_value();
                    std::stringstream len;
                    len << value.size();
                    res.version = req.version;
                    res.status = http::HttpStatus::OK;
                    http::HttpHeader cl = {};
                    cl.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                    cl.field_value = len.str();
                    res.headers = {
                        cl,
                        CONTROLLER_APP_COMMON_HTTP_HEADERS
                    };
                    http::HttpChunk nc = {};
                    nc.chunk_size = {value.size()};
                    nc.chunk_data = value;
                    res.chunks = {
                        nc
                    };
                }
            } else {
                res.version = req.version;
                res.status = http::HttpStatus::INTERNAL_SERVER_ERROR;
                http::HttpHeader cl = {};
                cl.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                cl.field_value = "33";
                res.headers = {
                    cl,
                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                };
                http::HttpChunk nc = {};
                nc.chunk_size = {33};
                nc.chunk_data = "{\"error\":\"Internal Server Error\"}";
                res.chunks = {
                    nc
                };
            }            
        } else if (ctx.route() == controller::resources::Routes::INIT){
            // The initialization route is alwasy singled threaded, so the following
            // operation is safe.
            http::HttpRequest& req = std::get<http::HttpRequest>(*(ctx.sessions().front()));
            if (initialized_) {
                res.version = req.version;
                res.status = http::HttpStatus::CONFLICT;
                http::HttpHeader cl = {};
                cl.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                cl.field_value = "0";
                res.headers = {
                    cl,
                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                };
                res.chunks = {
                    {}
                };
            } else {     
                res.version = req.version;
                res.status = http::HttpStatus::OK;
                http::HttpHeader cl = {};
                cl.field_name = http::HttpHeaderField::CONTENT_LENGTH;
                cl.field_value = "0";
                res.headers = {
                    cl,
                    CONTROLLER_APP_COMMON_HTTP_HEADERS
                };
                res.chunks = {
                    {}
                };
            }   
        }
        // Unknown routes are handled directly at the root of the application.
        return res;
    }

    void Controller::stop(){
        // struct timespec ts;
        // if(clock_gettime(CLOCK_REALTIME, &ts) == -1){
        //     std::cerr << "controller-app.cpp:1464:clock_gettime() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
        //     std::cout << "controller-app.cpp:1464:Controller::stop() called." << std::endl;
        // } else {
        //     std::cout << "controller-app.cpp:1467:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":Controller::stop() called." << std::endl;
        // }
        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_TERMINATE_EVENT, std::memory_order::memory_order_relaxed);
        io_mbox_ptr_->sched_signal_cv_ptr->notify_one();

        std::unique_lock<std::mutex> lk(*(controller_mbox_ptr_->sched_signal_mtx_ptr));
        controller_mbox_ptr_->sched_signal_cv_ptr->wait(lk);
    }

    Controller::~Controller()
    {
        stop();
    }
}// namespace app
}//namespace controller