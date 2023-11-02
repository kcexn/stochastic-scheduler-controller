#include "controller-app.hpp"
#include "../controller-events.hpp"
#include <application-servers/http/http-session.hpp>
#include "execution-context.hpp"
#include "action-relation.hpp"
#include "../resources/resources.hpp"
#include <charconv>
#include <transport-servers/sctp-server/sctp-session.hpp>
#include <sys/wait.h>
#include <curl/curl.h>

#define CONTROLLER_APP_COMMON_HTTP_HEADERS {http::HttpHeaderField::CONTENT_TYPE, "application/json", "", false, false, false, false, false, false},{http::HttpHeaderField::CONNECTION, "close", "", false, false, false, false, false, false},{http::HttpHeaderField::END_OF_HEADERS, "", "", false, false, false, false, false, false}

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

static void set_common_curl_handle_options(CURL* hnd, const std::string& url, const std::string& __OW_API_KEY, struct curl_slist* slist, FILE* writedata) {
    auto it = std::find(__OW_API_KEY.begin(), __OW_API_KEY.end(), ':');
    if(it == __OW_API_KEY.end()){
        std::cerr << "controller-app.cpp:108:delimiter ':' wasn't found." << std::endl;
        throw "what?";
    }
    std::string username(__OW_API_KEY.begin(), it);
    std::string password(++it, __OW_API_KEY.end());
    CURLcode status;
    switch(status = curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:119:setting CURLOPT_HTTPHEADER failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_URL, url.c_str()))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:127:setting CURLOPT_URL failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_USERNAME, username.c_str()))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:135:setting CURLOPT_USERNAME failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_PASSWORD, password.c_str()))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:143:setting CURLOPT_PASSWORD failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:151:setting CURLOPT_HTTP_VERSION failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_POST, 1))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:159:setting CURLOPT_POST failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.88.1"))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:167:setting CURLOPT_USERAGENT failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_WRITEDATA, writedata))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:175:setting CURLOPT_WRITEDATA failed:" << status << std::endl;
            throw "what?";
    }
    return;
}

static void set_curl_handle_options(CURL* hnd, const std::string& data){
    CURLcode status;
    switch(status = curl_easy_setopt(hnd, CURLOPT_COPYPOSTFIELDS, data.c_str()))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:188:setting CURLOPT_COPYPOSTFIELDS failed:" << status << std::endl;
            throw "what?";
    }
    switch(status = curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE, data.size()))
    {
        case CURLE_OK:
            break;
        default:
            std::cerr << "controller-app.cpp:196:setting CURLOPT_POSTFIELDSIZE failed:" << status << std::endl;
            throw "what?";
    }
    return;
}

static void populate_indices(std::vector<std::size_t>& indices, std::size_t manifest_size, std::size_t concurrency){
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
    return;
}

static void populate_request_data(boost::json::object& jctx, const std::shared_ptr<controller::app::ExecutionContext>& ctxp, const boost::json::value& val, const std::vector<std::size_t>& indices){
    boost::json::object jo;
    std::stringstream uuid;
    uuid << ctxp->execution_context_id();
    jo.emplace("uuid", boost::json::string(uuid.str()));
    std::vector<server::Remote> peers = ctxp->peer_addresses();
    boost::json::array ja;
    for(auto& peer: peers){
        ja.push_back(boost::json::string(rtostr(peer)));
    }
    jo.emplace("peers", ja);
    jo.emplace("value", val.at("value").as_object());
    // Emplace the first context index.
    jo.emplace("idx", indices[1]);
    jctx.emplace("execution_context", jo);
    return;
}

static void make_api_requests(const std::shared_ptr<controller::app::ExecutionContext>& ctxp, const boost::json::value& val){
    std::size_t concurrency = ctxp->manifest().concurrency();
    if(concurrency > 1){
        std::size_t manifest_size = ctxp->manifest().size();
        const char* __OW_ACTION_NAME = getenv("__OW_ACTION_NAME");
        if(__OW_ACTION_NAME == nullptr){
            std::cerr << "controller-app.cpp:240:__OW_ACTION_NAME envvar is not set!" << std::endl;
            throw "what?";
        }
        const char* __OW_API_HOST = getenv("__OW_API_HOST");
        if (__OW_API_HOST == nullptr){
            std::cerr << "controller-app.cpp:245:__OW_API_HOST envvar is not set!" << std::endl;
            throw "what?";
        }
        std::string __OW_API_KEY = ctxp->env()["__OW_API_KEY"];
        if(__OW_API_KEY.empty()){
            std::cerr << "controller-app.cpp:250:__OW_API_KEY envvar is not set!" << std::endl;
            throw "what?";
        }
        // Compute the index for each subsequent context by partitioning the manifest.
        std::vector<std::size_t> indices;
        populate_indices(indices, manifest_size, concurrency);

        // Initialize the request data for the context.
        boost::json::object jctx;
        populate_request_data(jctx, ctxp, val, indices);

        // Allocate space for the data in each subsequent request.
        std::vector<std::string> data_vec;
        data_vec.reserve(concurrency);
        data_vec.emplace_back(boost::json::serialize(jctx));

        // Construct the URL for the API requests.
        std::filesystem::path action_name(__OW_ACTION_NAME);
        std::string url(__OW_API_HOST);
        url.append("/api/v1/namespaces/");
        url.append(action_name.relative_path().begin()->string());
        url.append("/actions/");
        url.append(action_name.filename().string());

        // Construct the HTTP headers for the API requests.
        struct curl_slist* slist = nullptr;
        slist = curl_slist_append(slist, "Content-Type: application/json");
        slist = curl_slist_append(slist, "Accept: application/json");

        // We don't care about anything in the API responses so we will route everything to /dev/null.
        FILE* devnull = fopen("/dev/null", "w");
        if(devnull == nullptr){
            std::cerr << "controller-app.cpp:282:/dev/null failed to open for writing." << std::endl;
            throw "what?";
        }

        int num_running_handles = 0;
        CURLMcode mstatus;
        CURLM* mhnd = curl_multi_init();
        if(mhnd == nullptr){
            std::cerr << "controller-app.cpp:290:CURL multi init failed." << std::endl;
            throw "what?";
        }

        CURL* handle = curl_easy_init();
        if(handle){
            set_common_curl_handle_options(handle, url, __OW_API_KEY, slist, devnull);
        } else {
            std::cerr << "controller-app.cpp:298:curl_easy_init() failed." << std::endl;
            throw "what?";
        }
        for(std::size_t i = 2; i < concurrency; ++i){
            CURL* hnd = curl_easy_duphandle(handle);
            if(hnd){
                jctx["execution_context"].get_object()["idx"] = indices[i];
                data_vec.emplace_back(boost::json::serialize(jctx));
                set_curl_handle_options(hnd, data_vec.back());
                if(curl_multi_add_handle(mhnd, hnd) != CURLM_OK){
                    std::cerr << "controller-app.cpp:308:curl_multi_add_handle failed." << std::endl;
                    throw "what?";
                }
                mstatus = curl_multi_perform(mhnd, &num_running_handles);
                if(mstatus != CURLM_OK){
                    std::cerr << "controller-app.cpp:313:curl_multi_perform failed:" << mstatus << std::endl;
                    throw "what?";
                }
            } else {
                std::cerr << "controller-app.cpp:317:CURL easy handle initialization failed." << std::endl;
                throw "what?";
            }
        }
        set_curl_handle_options(handle, data_vec.front());
        if(curl_multi_add_handle(mhnd, handle) != CURLM_OK){
            std::cerr << "controller-app.cpp:323:curl_multi_add_handle failed." << std::endl;
            throw "what?";
        }
        mstatus = curl_multi_perform(mhnd, &num_running_handles);
        if(mstatus != CURLM_OK){
            std::cerr << "controller-app.cpp:328:curl_multi_perform failed:" << mstatus << std::endl;
            throw "what?";
        }
        if(num_running_handles > 0){
            try{
                std::thread curl([&](int num_running_handles, struct curl_slist* slist, CURLM* mhnd, FILE* writedata){
                    // After the API requests have been made, I really don't care when or how long it takes for the 
                    // Openwhisk servers to respond. The only responsibility this thread has is to gracefully release all of the
                    // resources that were created to make the HTTP requests. Since this is the case, we are going to 
                    // add a nice value to this thread so that it doesn't take up as much of the scheduler time shares.
                    errno = 0;
                    if(nice(10) == -1 && errno > 0){
                        std::cerr << "controller-app.cpp:340:nice() failed:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        throw "what?";
                    }
                    int numfds = 0;
                    struct CURLMsg* m = nullptr;
                    CURLMcode mstatus;
                    do{
                        mstatus = curl_multi_poll(mhnd, nullptr, 0, 1000, &numfds);
                        if(mstatus != CURLM_OK){
                            std::cerr << "controller-app.cpp:349:curl_multi_poll failed:" << mstatus << std::endl;
                            throw "what?";
                        }
                        if(numfds > 0){
                            mstatus = curl_multi_perform(mhnd, &num_running_handles);
                            if(mstatus != CURLM_OK){
                                std::cerr << "controller-app.cpp:355:curl_multi_perform failed:" << mstatus << std::endl;
                                throw "what?";
                            }
                            int msgq = 0;
                            do{
                                m = curl_multi_info_read(mhnd, &msgq);
                                if(m && (m->msg == CURLMSG_DONE)){
                                    CURL* hnd = m->easy_handle;
                                    switch(curl_multi_remove_handle(mhnd, hnd))
                                    {
                                        case CURLM_OK:
                                            break;
                                        default:
                                            std::cerr << "controller-app.cpp:368:curl_multi_remove_handle() failed." << std::endl;
                                            throw "what?";
                                    }
                                    curl_easy_cleanup(hnd);
                                }
                            }while(msgq > 0);
                        }
                    }while(num_running_handles > 0);
                    switch(curl_multi_cleanup(mhnd))
                    {
                        case CURLM_OK:
                            break;
                        default:
                            std::cerr << "controller-app.cpp:381:curl_multi_cleanup() failed." << std::endl;
                            throw "what?";
                    }
                    curl_slist_free_all(slist);
                    fclose(writedata);
                    return;
                }, num_running_handles, slist, mhnd, devnull);
                curl.detach();
            } catch(std::system_error& e){
                std::cerr << "controller-app.cpp:390:curl thread failed to start." << std::endl;
                throw e;
            }
        } else {
            struct CURLMsg* m = nullptr;
            int msgq = 0;
            do{
                m = curl_multi_info_read(mhnd, &msgq);
                if(m && (m->msg == CURLMSG_DONE)){
                    CURL* hnd = m->easy_handle;
                    switch(curl_multi_remove_handle(mhnd, hnd))
                    {
                        case CURLM_OK:
                            break;
                        default:
                            std::cerr << "controller-app.cpp:405:curl_multi_remove_handle() failed." << std::endl;
                            throw "what?";
                    }
                    curl_easy_cleanup(hnd);
                }
            }while(msgq > 0);
            switch(curl_multi_cleanup(mhnd))
            {
                case CURLM_OK:
                    break;
                default:
                    std::cerr << "controller-app.cpp:416:curl_multi_cleanup() failed." << std::endl;
                    throw "what?";
            }
            curl_slist_free_all(slist);
            fclose(devnull);
        }
    }
    return;
}


namespace controller{
namespace app{
    Controller::Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<controller::io::MessageBox>()),
        io_(io_mbox_ptr_, "/run/controller/controller.sock", ioc),
        ioc_(ioc)
    {
        CURLcode status = curl_global_init(CURL_GLOBAL_NOTHING); // Don't plan on using SSL support.
        if(status != CURLE_OK){
            std::cerr << "controller-app.cpp:438:libcurl global initialization failed with error code:" << status << std::endl;
            throw "what?";
        }
        try{
            std::thread application(
                &Controller::start, this
            );
            tid_ = application.native_handle();
            application.detach();
        } catch (std::system_error& e){
            std::cerr << "controller-app.cpp:448:application failed to start with error:" << e.what() << std::endl;
            throw e;
        }
    }

    Controller::Controller(std::shared_ptr<controller::io::MessageBox> mbox_ptr, boost::asio::io_context& ioc, const std::filesystem::path& upath, std::uint16_t sport)
      : controller_mbox_ptr_(mbox_ptr),
        initialized_{false},
        io_mbox_ptr_(std::make_shared<controller::io::MessageBox>()),
        io_(io_mbox_ptr_, upath.string(), ioc, sport),
        ioc_(ioc)
    {
        CURLcode status = curl_global_init(CURL_GLOBAL_NOTHING); // Don't plan on using SSL support.
        if(status != CURLE_OK){
            std::cerr << "controller-app.cpp:462:libcurl global initialization failed with error code:" << status << std::endl;
            throw "what?";
        }
        try{
            std::thread application(
                &Controller::start, this
            );
            tid_ = application.native_handle();
            application.detach();
        } catch (std::system_error& e){
            std::cerr << "controller-app.cpp:472:application failed to start with error:" << e.what() << std::endl;
            throw e;
        }
    }

    void Controller::start(){
        // Initialize resources I might need.
        std::unique_lock<std::mutex> lk(*(io_mbox_ptr_->sched_signal_mtx_ptr), std::defer_lock);
        std::uint16_t thread_local_signal;
        std::shared_ptr<server::Session> server_session;
        // struct timespec troute[2] = {};
        // struct timespec tschedend[2] = {};
        // struct timespec ts[2] = {};
        // struct timespec ts = {};
        // Scheduling Loop.
        // The TERMINATE signal once set, will never be cleared, so memory_order_relaxed synchronization is a sufficient check for this. (I'm pretty sure.)
        while(true){
            server_session = std::shared_ptr<server::Session>();
            lk.lock();
            io_mbox_ptr_->sched_signal_cv_ptr->wait_for(lk, std::chrono::milliseconds(100), [&]{ 
                return (io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed) || (io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed) & ~CTL_TERMINATE_EVENT)); 
            });
            thread_local_signal = io_mbox_ptr_->sched_signal_ptr->load(std::memory_order::memory_order_relaxed);
            if(thread_local_signal & ~CTL_TERMINATE_EVENT){
                io_mbox_ptr_->sched_signal_ptr->fetch_and(~(thread_local_signal & ~CTL_TERMINATE_EVENT), std::memory_order::memory_order_relaxed);
            }
            if(io_mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed)){
                io_mbox_ptr_->msg_flag.store(false, std::memory_order::memory_order_relaxed);
                server_session = io_mbox_ptr_->session;
            }
            // if(status){
            //     clock_gettime(CLOCK_REALTIME, &ts);
            //     std::cout << "controller-app.cpp:165:msg_flag read:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
            // }
            lk.unlock();
            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
            if(server_session){
                // clock_gettime(CLOCK_MONOTONIC, &troute[0]);
                std::shared_ptr<http::HttpSession> http_session_ptr;
                auto http_client = std::find_if(hcs_.begin(), hcs_.end(), [&](auto& hc){
                    return *hc == server_session;
                });
                if(http_client != hcs_.end()){
                    
                    /* if it is in the http client server list, then we treat this as an incoming response to a client session. */
                    std::shared_ptr<http::HttpClientSession> http_client_ptr = std::static_pointer_cast<http::HttpClientSession>(*http_client);
                    http_client_ptr->read();
                    route_response(http_client_ptr);
                    // clock_gettime(CLOCK_MONOTONIC, &troute[1]);
                    // std::cout << "Client Routing time:" << (troute[1].tv_sec*1000000 + troute[1].tv_nsec/1000) - (troute[0].tv_sec*1000000 + troute[0].tv_nsec/1000) << std::endl;
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
                            // clock_gettime(CLOCK_MONOTONIC, &troute[1]);
                            // std::cout << "Server Routing time - new session:" << (troute[1].tv_sec*1000000 + troute[1].tv_nsec/1000) - (troute[0].tv_sec*1000000 + troute[0].tv_nsec/1000) << std::endl;
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
                            // clock_gettime(CLOCK_MONOTONIC, &troute[1]);
                            // std::cout << "Server Routing time - existing session:" << (troute[1].tv_sec*1000000 + troute[1].tv_nsec/1000) - (troute[0].tv_sec*1000000 + troute[0].tv_nsec/1000) << std::endl;
                        }
                    }
                }
            }
            // if(status){
            //     clock_gettime(CLOCK_REALTIME, &ts);
            //     std::cout << "controller-app.cpp:219:http streams routed:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
            // }
            if(thread_local_signal & CTL_TERMINATE_EVENT){
                if(hs_.empty() && ctx_ptrs.empty() && hcs_.empty()){
                    controller_mbox_ptr_->sched_signal_cv_ptr->notify_all();
                    break;
                }
            }
            // if(status){
            //     clock_gettime(CLOCK_REALTIME, &ts);
            //     std::cout << "controller-app.cpp:229:terminate processed:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
            // }
            if (thread_local_signal & CTL_IO_SCHED_END_EVENT){
                // clock_gettime(CLOCK_REALTIME, &ts);
                // std::cout << "controller-app.cpp:233:sched end processing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

                while(waitpid(-1, nullptr, WNOHANG) > 0){}
                // Find stopped contexts:
                auto stopped = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& ctxp){
                    return (ctxp->is_stopped());
                });
                while(stopped != ctx_ptrs.end()){
                    auto& ctxp = *stopped;
                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:243:stopped context processing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

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
                        res.chunks.emplace_back();
                        res.chunks.back().chunk_size = {1};
                        res.chunks.back().chunk_data = "]"; // Close the JSON stream array.
                        res.chunks.emplace_back(); // Close the HTTP stream.
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
                        req.chunks.emplace_back();
                        req.chunks.back().chunk_size = {1};
                        req.chunks.back().chunk_data = "]"; // Close the JSON stream array.
                        req.chunks.emplace_back(); // Close the HTTP stream.
                        next_session->set(rr);
                        next_session->write([&,next_session](){
                            return;
                        });
                        ctxp->peer_client_sessions().pop_back();
                    }
                    ctx_ptrs.erase(stopped); // This invalidates the iterator in the loop, so we have to perform the original search again.
                    // Find a context that has a stopped thread.
                    stopped = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& ctxp){
                        return (ctxp->is_stopped());
                    });

                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:296:stopped context processing finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                }

                // Find contexts that have stopped threads but are not stopped.
                auto updated = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](std::shared_ptr<ExecutionContext>& ctx_ptr){
                    auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](ThreadControls& thread){
                        return thread.is_stopped() && thread.has_pending_idxs();
                    });
                    return (tmp == ctx_ptr->thread_controls().end())? false : true;
                });
                while(updated != ctx_ptrs.end()){
                    auto& ctxp = *updated;
                    // Check to see if the context is stopped.
                    // std::string __OW_ACTIVATION_ID = (*it)->env()["__OW_ACTIVATION_ID"];
                    // if(!__OW_ACTIVATION_ID.empty()){
                    //     struct timespec ts = {};
                    //     clock_gettime(CLOCK_REALTIME, &ts);
                    //     std::cout << "controller-app.cpp:220:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":__OW_ACTIVATION_ID=" << __OW_ACTIVATION_ID << std::endl;
                    // }
                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:316:stopped thread processing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

                    // Evaluate which thread to execute next and notify it.
                    auto stopped_thread = std::find_if(ctxp->thread_controls().begin(), ctxp->thread_controls().end(), [&](auto& thread){
                        return thread.is_stopped() && thread.has_pending_idxs();
                    });
                    // invalidate the thread.
                    std::vector<std::size_t> execution_context_idxs = stopped_thread->pop_idxs();
                    // Do the subsequent steps only if there are execution idxs that exited normally.
                    // get the index of the stopped thread.
                    std::ptrdiff_t idx = stopped_thread - ctxp->thread_controls().begin();
                    
                    /* For every peer in the peer table notify them with a state update */
                    // It's only worth constructing the state update values if there are peers to update.
                    if( (ctxp->peer_client_sessions().size() + ctxp->peer_server_sessions().size()) > 0 ){
                        std::shared_ptr<Relation>& finished = ctxp->manifest()[idx];
                        std::string f_key(finished->key());
                        std::string f_val(finished->acquire_value());
                        finished->release_value();
                        boost::json::object jo;
                        boost::json::error_code ec;
                        boost::json::value jv = boost::json::parse(f_val, ec);
                        if(ec){
                            std::cerr << "controller-app.cpp:678:JSON parsing failed:" << ec.message() << ":value:" << f_val << std::endl;
                            throw "This shouldn't happen.";
                        }
                        jo.emplace(f_key, jv);
                        boost::json::object jf_val;
                        jf_val.emplace("result", jo);
                        std::string data(",");
                        std::string jsonf_val = boost::json::serialize(jf_val);
                        data.append(jsonf_val);
                        for(auto& peer_session: ctxp->peer_client_sessions()){
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
                        for(auto& peer_session: ctxp->peer_server_sessions()){
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
                    std::string key(ctxp->manifest()[(++idx)%(ctxp->thread_controls().size())]->key());
                    for (auto& idx: execution_context_idxs){
                        // Get the next relation to execute from the dependencies of the relation at this key.
                        std::shared_ptr<Relation> next = ctxp->manifest().next(key, idx);
                        // Retrieve the index of this relation in the manifest.
                        auto next_it = std::find_if(ctxp->manifest().begin(), ctxp->manifest().end(), [&](auto& rel){
                            return rel->key() == next->key();
                        });
                        std::ptrdiff_t next_idx = next_it - ctxp->manifest().begin();
                        //Start the thread at this index.
                        ctxp->thread_controls()[next_idx].notify(idx);
                    }
                    // Search through remaining contexts.
                    updated = std::find_if(++updated, ctx_ptrs.end(), [&](auto& ctx_ptr){
                        auto tmp = std::find_if(ctx_ptr->thread_controls().begin(), ctx_ptr->thread_controls().end(), [&](auto& thread){
                            return thread.is_stopped() && thread.has_pending_idxs();
                        });
                        return (tmp == ctx_ptr->thread_controls().end())? false : true;
                    });
                    
                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:393:stopped thread processing finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                }
            }
            // if(status){
            //     clock_gettime(CLOCK_MONOTONIC, &ts[1]);
            //     std::cout << "controller-app.cpp:407:loop time:" << (ts[1].tv_sec*1000000 + ts[1].tv_nsec/1000) - (ts[0].tv_sec*1000000 + ts[0].tv_nsec/1000) << std::endl;
            // }
        }
        return;
    }

    void Controller::route_response(std::shared_ptr<http::HttpClientSession>& session){
        // struct timespec ts = {};
        // clock_gettime(CLOCK_REALTIME, &ts);
        // std::cout << "controller-app.cpp:396:route_response started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

        http::HttpReqRes req_res = session->get();
        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);
        auto it = std::find_if(req.headers.begin(), req.headers.end(), [&](auto& header){
            return (header.field_name == http::HttpHeaderField::CONTENT_LENGTH);
        });
        if(res.status == http::HttpStatus::CREATED){
            // clock_gettime(CLOCK_REALTIME, &ts);
            // std::cout << "controller-app.cpp:406:http:HttpStatus::Created started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

            for(; res.pos < res.next_chunk; ++res.pos){
                // clock_gettime(CLOCK_REALTIME, &ts);
                // std::cout << "controller-app.cpp:410:chunk processing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

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
                        std::cerr << "controller-app.cpp:773:json_obj_str is empty" << std::endl;
                        continue;
                    } else if (json_obj_str.front() == ']'){
                        /* Function is complete. Terminate the execution context */
                        auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                            auto tmp = std::find(ctx_ptr->peer_client_sessions().begin(), ctx_ptr->peer_client_sessions().end(), session);
                            return (tmp == ctx_ptr->peer_client_sessions().end()) ? false : true;
                        });
                        if(server_ctx != ctx_ptrs.end()){
                            std::vector<std::thread> stops;
                            for(auto& thread_control: (*server_ctx)->thread_controls()){
                                try{
                                    stops.emplace_back([&](){
                                        thread_control.stop_thread();
                                    });
                                } catch (std::system_error& e){
                                    std::cerr << "controller-app.cpp:789:stop thread failed to start with error:" << e.what() << std::endl;
                                    throw e;
                                }
                            }
                            for(auto& stop: stops){
                                stop.join();
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
                        std::cerr << "controller-app.cpp:824:JSON Parsing failed:" << ec.message() <<":value:" << json_obj_str << std::endl;
                        throw "this shouldn't happen.";
                    }
                    auto ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto& cp){
                        auto tmp = std::find(cp->peer_client_sessions().begin(), cp->peer_client_sessions().end(), session);
                        return (tmp == cp->peer_client_sessions().end()) ? false : true;
                    });

                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:486:JSON parsed:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

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
                            std::shared_ptr<controller::app::ExecutionContext>& ctx_ptr = *ctx;
                            boost::json::object jo;
                            UUID::Uuid uuid = ctx_ptr->execution_context_id();
                            std::stringstream uuid_str;
                            uuid_str << uuid;
                            jo.emplace("uuid", boost::json::string(uuid_str.str()));
                            boost::json::array pja;
                            for(auto& peer: new_peers){                                                         
                                pja.push_back(boost::json::string(rtostr(peer)));
                            }                                                           
                            jo.emplace("peers", pja);
                            
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
                            for(auto& peer: new_peers){
                                auto it = std::find_if(old_peers.begin(), old_peers.end(), [&](auto& p){
                                    return (p.ipv4_addr.address.sin_addr.s_addr == peer.ipv4_addr.address.sin_addr.s_addr && p.ipv4_addr.address.sin_port == peer.ipv4_addr.address.sin_port);
                                });
                                if(it == old_peers.end()){
                                    io_.async_connect(peer, [&, nreq, ctx_ptr](const boost::system::error_code& ec, const std::shared_ptr<server::Session>& t_session){
                                        if(!ec){
                                            std::shared_ptr<http::HttpClientSession> client_session = std::make_shared<http::HttpClientSession>(hcs_, t_session);
                                            hcs_.push_back(client_session);
                                            ctx_ptr->peer_client_sessions().push_back(client_session);  
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

                            auto& ctxp = *ctx;
                            std::ptrdiff_t idx = rel - (ctxp)->manifest().begin();
                            std::size_t manifest_size = ctxp->manifest().size();
                            std::shared_ptr<std::condition_variable> wait_cv_ = std::make_shared<std::condition_variable>();
                            std::shared_ptr<std::atomic<bool> > wflag = std::make_shared<std::atomic<bool> >();
                            wflag->store(false, std::memory_order::memory_order_relaxed);
                            try{
                                std::thread reschedule([&, ctxp, idx, manifest_size, wait_cv_, wflag](){
                                    auto& thread = ctxp->thread_controls()[idx];
                                    // This operation can block.
                                    auto execution_idxs = thread.stop_thread();
                                    /* Reschedule if necessary */
                                    for(auto& i: execution_idxs){
                                        // Get the starting relation.
                                        std::string start_key(ctxp->manifest()[i % manifest_size]->key());
                                        std::shared_ptr<Relation> start = ctxp->manifest().next(start_key, i);
                                        if(start->key().empty()){
                                            // If the start key is empty, that means that all tasks in the schedule are complete.
                                            for(auto& thread_control: ctxp->thread_controls()){
                                                thread_control.stop_thread();
                                            }
                                            for(auto& rel: ctxp->manifest()){
                                                auto& value = rel->acquire_value();
                                                if(value.empty()){
                                                    value = "{}";
                                                }
                                                rel->release_value();
                                            }
                                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                            wait_cv_->notify_one();
                                            return;
                                        } else {
                                            // Find the index in the manifest of the starting relation.
                                            auto start_it = std::find_if(ctxp->manifest().begin(), ctxp->manifest().end(), [&](auto& rel){
                                                return rel->key() == start->key();
                                            });
                                            if(start_it == ctxp->manifest().end()){
                                                std::cerr << "Relation does not exist in the active manifest." << std::endl;
                                                throw "This shouldn't be possible";
                                            }
                                            std::ptrdiff_t start_idx = start_it - ctxp->manifest().begin();
                                            ctxp->thread_controls()[start_idx].notify(i);
                                        }                 
                                    }
                                    wflag->store(true, std::memory_order::memory_order_relaxed);
                                    wait_cv_->notify_one();
                                    return;
                                });
                                std::mutex wait_mtx_;
                                std::unique_lock<std::mutex> lk(wait_mtx_);
                                if(wait_cv_->wait_for(lk, std::chrono::milliseconds(10), [&](){return wflag->load(std::memory_order::memory_order_relaxed);})){
                                    reschedule.join();
                                } else {
                                    reschedule.detach();
                                }
                            } catch (std::system_error& e){
                                std::cerr << "controller-app.cpp:970:reschedule failed to start with error:" << e.what() << std::endl;
                                throw e;
                            }
                        }
                    }

                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:629:results handled:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                }

                // clock_gettime(CLOCK_REALTIME, &ts);
                // std::cout << "controller-app.cpp:633:chunk processing finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
            }
            http::HttpReqRes rr = session->get();
            std::get<http::HttpResponse>(rr) = res;
            session->set(rr);

            // clock_gettime(CLOCK_REALTIME, &ts);
            // std::cout << "controller-app.cpp:640:http:HttpStatus::Created started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
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
                    for(std::size_t i=0; i < ctx_ptr->thread_controls().size(); ++i){
                        auto& thread = ctx_ptr->thread_controls()[i];
                        auto& relation = ctx_ptr->manifest()[i];
                        auto& value = relation->acquire_value();
                        if(value.empty()){
                            value = "{}";
                        }
                        relation->release_value();
                        thread.stop_thread();
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
        // struct timespec ts = {};
        // clock_gettime(CLOCK_REALTIME, &ts);
        // std::cout << "controller-app.cpp:691:route_request started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

        http::HttpReqRes req_res = session->get();
        http::HttpRequest& req = std::get<http::HttpRequest>(req_res);            
        // Start processing chunks, iff next_chunk > 0.
        if(req.route == "/run" || req.route == "/init"){
            // clock_gettime(CLOCK_REALTIME, &ts);
            // std::cout << "controller-app.cpp:698:path routing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
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
                // clock_gettime(CLOCK_REALTIME, &ts);
                // std::cout << "controller-app.cpp:716:chunk processing started:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                std::size_t next_comma = 0;
                const http::HttpChunk& chunk = req.chunks[req.pos];
                const http::HttpBigNum& chunk_size = chunk.chunk_size;

                if(chunk_size == http::HttpBigNum{0}){
                    // A 0 length chunk means that the entire HTTP Request has been consumed.
                    // We have to maintain the session until the entire HTTP response has been completely processed.
                    return;
                }

                while(http::HttpBigNum{next_comma} < chunk_size){
                    // struct timespec tjson[2] = {};
                    // clock_gettime(CLOCK_MONOTONIC, &tjson[0]);
                    std::string_view json_obj_str = find_next_json_object(chunk.chunk_data, next_comma);
                    if(json_obj_str.empty()){
                        std::cerr << "controller-app.cpp:1063:json_obj_str is empty" << std::endl;
                        continue;
                    } else if(json_obj_str.front() == ']'){
                        /* Function is complete. Terminate the execution context */
                        auto server_ctx = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                            auto tmp = std::find(ctx_ptr->peer_server_sessions().begin(), ctx_ptr->peer_server_sessions().end(), session);
                            return (tmp == ctx_ptr->peer_server_sessions().end()) ? false : true;
                        });
                        if(server_ctx != ctx_ptrs.end()){
                            auto& ctxp = *server_ctx;
                            std::vector<std::thread> stops;
                            for(auto& thread_control: ctxp->thread_controls()){
                                try{
                                    stops.emplace_back([&](){
                                        thread_control.stop_thread();
                                    });
                                } catch(std::system_error& e){
                                    std::cerr << "controller-app.cpp:1089:stops thread failed to start with error:" << e.what() << std::endl;
                                    throw e;
                                }
                            }
                            for(auto& stop: stops){
                                stop.join();
                            }
                            for (auto& rel: ctxp->manifest()){
                                auto& value = rel->acquire_value();
                                if(value.empty()){
                                    value = "{}";
                                }
                                rel->release_value();
                            } 
                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                            // clock_gettime(CLOCK_MONOTONIC, &tjson[1]);
                            // std::cout << "route request ] termination time: " << (tjson[1].tv_sec*1000000 + tjson[1].tv_nsec/1000) - (tjson[0].tv_sec*1000000 + tjson[0].tv_nsec/1000) << std::endl;
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
                        std::cerr << "controller-app.cpp:1119:JSON parsing failed:" << ec.message() << ":value:" << json_obj_str << std::endl;
                        throw "Json Parsing failed.";
                    }

                    // clock_gettime(CLOCK_REALTIME, &ts);
                    // std::cout << "controller-app.cpp:787:JSON parsing finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;

                    if(req.route == "/run"){
                        if(req.verb == http::HttpVerb::POST){
                            // clock_gettime(CLOCK_REALTIME, &ts);
                            // std::cout << "controller-app.cpp:792:/run POST:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                            // struct timespec ts[2] = {};
                            // clock_gettime(CLOCK_MONOTONIC, &ts[0]);
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
                                            for(std::size_t i=0; i < ctx_ptr->thread_controls().size(); ++i){
                                                auto& thread = ctx_ptr->thread_controls()[i];
                                                auto& relation = ctx_ptr->manifest()[i];
                                                relation->acquire_value() = "{}";
                                                relation->release_value();
                                                thread.signal().store(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                            }
                                            io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                            io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                            return;                   
                                        }
                                    }
                                    #endif
                                    const std::size_t& manifest_size = ctx_ptr->manifest().size();
                                    // This id is pushed in the context constructor.
                                    std::size_t execution_idx = ctx_ptr->pop_execution_idx();
                                    // Get the starting relation.
                                    std::string start_key(ctx_ptr->manifest()[execution_idx % manifest_size]->key());
                                    std::shared_ptr<Relation> start = ctx_ptr->manifest().next(start_key, execution_idx);

                                    // Find the index in the manifest of the starting relation.
                                    auto start_it = std::find_if(ctx_ptr->manifest().begin(), ctx_ptr->manifest().end(), [&](auto& rel){
                                        return rel->key() == start->key();
                                    });
                                    if (start_it == ctx_ptr->manifest().end()){
                                        std::cerr << "controller-app.cpp:1216:there are no matches for rel->key() == start->key():start->key()=" << start->key() << std::endl;
                                        // If the start key is past the end of the manifest, that means that
                                        // there are no more relations to complete execution. Simply signal a SCHED_END condition and return from request routing.
                                        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                        io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                        return;
                                    }
                                    std::ptrdiff_t start_idx = start_it - ctx_ptr->manifest().begin();
                                    std::shared_ptr<std::condition_variable> sync_ = std::make_shared<std::condition_variable>();
                                    std::shared_ptr<std::atomic<bool> > sflag = std::make_shared<std::atomic<bool> >();
                                    sflag->store(false, std::memory_order::memory_order_relaxed);
                                    try{
                                        std::thread initializer(
                                            [&, ctx_ptr, manifest_size, start_idx, run, sync_, sflag](std::shared_ptr<controller::io::MessageBox> mbox_ptr){
                                                for(std::size_t i=0; i < manifest_size; ++i){
                                                    auto& thread_control = ctx_ptr->thread_controls()[(i+start_idx) % manifest_size];
                                                    if(thread_control.is_stopped()){
                                                        mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                        continue;
                                                    }
                                                    // Fork exec the executor subprocess.
                                                    if(thread_control.thread_continue()){
                                                        if(thread_control.is_stopped()){
                                                            thread_control.cleanup();
                                                            mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                            mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                            continue;
                                                        }
                                                    } else {
                                                        thread_control.cleanup();
                                                        continue;
                                                    }
                                                    try{
                                                        std::thread executor(
                                                            [&, ctx_ptr, i, manifest_size, start_idx, mbox_ptr](){
                                                                auto& thread_control = ctx_ptr->thread_controls()[(i+start_idx) % manifest_size];
                                                                // The first continue synchronizes the controller with the exec'd launcher.
                                                                if(thread_control.thread_continue()){
                                                                    if(thread_control.is_stopped()){
                                                                        thread_control.cleanup();
                                                                        mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                                        mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                                        return;
                                                                    }
                                                                }
                                                                // After we are synchronized with the exec'd launcher, we send a SIGSTOP to the subprocess group, and 
                                                                // block the executor thread until further notice.
                                                                thread_control.wait();
                                                                if(thread_control.is_stopped()){
                                                                    thread_control.cleanup();
                                                                    mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                                    mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                                    return;
                                                                }
                                                                // After the thread is unblocked continue running until the function has completed, or until
                                                                // we are preempted.
                                                                while(thread_control.thread_continue()){
                                                                    if(thread_control.is_stopped()){
                                                                        thread_control.cleanup();
                                                                        mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                                        mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                                        return;
                                                                    }
                                                                }
                                                                thread_control.signal().fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                                mbox_ptr->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                                mbox_ptr->sched_signal_cv_ptr->notify_one();
                                                                return;
                                                            }
                                                        );
                                                        executor.detach();
                                                    } catch (std::system_error& e){
                                                        std::cerr << "controller-app.cpp:1289:executor failed to start with error:" << e.what() << std::endl;
                                                        throw e;
                                                    }
                                                }
                                                sflag->store(true, std::memory_order::memory_order_relaxed);
                                                sync_->notify_one();
                                                return;
                                            }, io_mbox_ptr_
                                        );
                                        ctx_ptr->thread_controls()[start_idx].notify(execution_idx);
                                        std::mutex smtx;
                                        std::unique_lock<std::mutex> slk(smtx);
                                        if(sync_->wait_for(slk, std::chrono::milliseconds(20), [&](){ return sflag->load(std::memory_order::memory_order_relaxed); })){
                                            initializer.join();
                                        } else {
                                            initializer.detach();
                                        }
                                        slk.unlock();
                                    } catch(std::system_error& e){
                                        std::cerr << "controller-app.cpp:1308:initializer failed to start with error:" << e.what() << std::endl;
                                        throw e;
                                    }
                                    /* Initialize the http client sessions */
                                    if(ctx_ptr->execution_context_idx_array().front() == 0){
                                        /* This is the primary context */
                                        // The primary context will have no client peer connections, only server peer connections.
                                        // The primary context must hit the OW API endpoint `concurrency' no. of times with the
                                        // a different execution context idx and the same execution context id each time.
                                        make_api_requests(ctx_ptr, val);
                                    } else {
                                        /* This is a secondary context */
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
                                        for(auto& peer: ctx_ptr->peer_addresses()){
                                            if(peer.ipv4_addr.address.sin_addr.s_addr != io_.local_sctp_address.ipv4_addr.address.sin_addr.s_addr || peer.ipv4_addr.address.sin_port != io_.local_sctp_address.ipv4_addr.address.sin_port){   
                                                io_.async_connect(peer, [&, ctx_ptr, nreq](const boost::system::error_code& ec, const std::shared_ptr<server::Session>& t_session){
                                                    if(!ec){
                                                        std::shared_ptr<http::HttpClientSession> client_session = std::make_shared<http::HttpClientSession>(hcs_, t_session);
                                                        hcs_.push_back(client_session);
                                                        ctx_ptr->peer_client_sessions().push_back(client_session);
                                                        std::get<http::HttpRequest>(*client_session) = nreq;
                                                        client_session->write([&, client_session](){ return; });
                                                    }
                                                });
                                            }
                                        }
                                    }
                                }
                            } else {
                                // invalidate the fibers.
                                std::cerr << "controller-app.cpp:1368:/run route reached before initialization." << std::endl;
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
                            // clock_gettime(CLOCK_MONOTONIC, &ts[1]);
                            // std::cout << "Run route - POST time: " << (ts[1].tv_sec*1000000 + ts[1].tv_nsec/1000) - (ts[0].tv_sec*1000000 + ts[0].tv_nsec/1000) << std::endl;
                        } else if(req.verb == http::HttpVerb::PUT){
                            // clock_gettime(CLOCK_REALTIME, &ts);
                            // std::cout << "controller-app.cpp:1141:/run PUT:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                            // struct timespec ts[2] = {};
                            // clock_gettime(CLOCK_MONOTONIC, &ts[0]);
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
                                                std::cerr << "controller-app.cpp:1427:JSON parsing failed:" << ec.message() << ":value:" << value << std::endl;
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
                                    res.chunks.emplace_back();
                                    res.chunks.back().chunk_size = {1};
                                    res.chunks.back().chunk_data = "]";
                                    res.chunks.emplace_back();
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
                                        auto& ctxp = *server_ctx;
                                        std::ptrdiff_t idx = relation - ctxp->manifest().begin();
                                        std::size_t manifest_size = ctxp->manifest().size();
                                        std::shared_ptr<std::condition_variable> wait_cv_ = std::make_shared<std::condition_variable>();
                                        std::shared_ptr<std::atomic<bool> > wflag = std::make_shared<std::atomic<bool> >();
                                        wflag->store(false, std::memory_order::memory_order_relaxed);
                                        try{
                                            std::thread reschedule([&, ctxp, idx, manifest_size, wait_cv_, wflag](){
                                                auto& thread = ctxp->thread_controls()[idx];    
                                                auto execution_idxs = thread.stop_thread();
                                                for(auto& i: execution_idxs){
                                                    // Get the starting relation.
                                                    std::string start_key(ctxp->manifest()[i % manifest_size]->key());
                                                    std::shared_ptr<Relation> start = ctxp->manifest().next(start_key, i);
                                                    if(start->key().empty()){
                                                        // If the start key is empty, that means that all tasks in the schedule are complete.
                                                        for(auto& thread_control: ctxp->thread_controls()){
                                                            thread_control.stop_thread();
                                                        }
                                                        for(auto& rel: ctxp->manifest()){
                                                            auto& value = rel->acquire_value();
                                                            if(value.empty()){
                                                                value = "{}";
                                                            }
                                                            rel->release_value();
                                                        }
                                                        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_IO_SCHED_END_EVENT, std::memory_order::memory_order_relaxed);
                                                        io_mbox_ptr_->sched_signal_cv_ptr->notify_one();
                                                        wait_cv_->notify_one();
                                                        return;
                                                    } else {
                                                        // Find the index in the manifest of the starting relation.
                                                        auto start_it = std::find_if(ctxp->manifest().begin(), ctxp->manifest().end(), [&](auto& rel){
                                                            return rel->key() == start->key();
                                                        });
                                                        if(start_it == ctxp->manifest().end()){
                                                            std::cerr << "Relation does not exist in the active manifest." << std::endl;
                                                            throw "This shouldn't be possible";
                                                        }
                                                        std::ptrdiff_t start_idx = start_it - ctxp->manifest().begin();
                                                        ctxp->thread_controls()[start_idx].notify(i);
                                                    }             
                                                }
                                                wflag->store(true, std::memory_order::memory_order_relaxed);
                                                wait_cv_->notify_one();
                                                return;
                                            });
                                            std::mutex wait_mtx_;
                                            std::unique_lock lk(wait_mtx_);
                                            if(wait_cv_->wait_for(lk, std::chrono::milliseconds(10), [&](){return wflag->load(std::memory_order::memory_order_relaxed);})){
                                                reschedule.join();
                                            } else {
                                                reschedule.detach();
                                            }
                                        } catch(std::system_error& e){
                                            std::cerr << "controller-app.cpp:1584:reschedule failed to start with error:" << e.what() << std::endl;
                                            throw e;
                                        }
                                    }
                                }
                            }
                            // clock_gettime(CLOCK_MONOTONIC, &ts[1]);
                            // std::cout << "Run route - PUT time: " << (ts[1].tv_sec*1000000 + ts[1].tv_nsec/1000) - (ts[0].tv_sec*1000000 + ts[0].tv_nsec/1000) << std::endl;
                        }                 
                    } else if (req.route == "/init"){
                        // clock_gettime(CLOCK_REALTIME, &ts);
                        // std::cout << "controller-app.cpp:1348:/init POST:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
                        // struct timespec ts[2] = {};
                        // clock_gettime(CLOCK_MONOTONIC, &ts[0]);
                        controller::resources::init::Request init(val.as_object());
                        // It is not strictly necessary to construct a context for initialization requests.
                        // But it keeps the controller resource interface homogeneous and easy to follow.
                        // Also, since the initialization route is only called once, the cost to performance
                        // should not be significant.
                        std::shared_ptr<ExecutionContext> ctx_ptr = controller::resources::init::handle();
                        auto http_it = std::find_if(ctx_ptr->sessions().cbegin(), ctx_ptr->sessions().cend(), [&](auto& ptr){
                            return ptr == session;
                        });
                        if(http_it == ctx_ptr->sessions().cend()){
                            ctx_ptr->sessions().push_back(session);
                        }
                        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
                        res = create_response(*ctx_ptr);
                        if(!initialized_) {
                            // Execute the initializer.
                            if ( res.status == http::HttpStatus::OK ){
                                init.run();
                                initialized_ = true;
                            }
                        }
                        session->write(
                            req_res,
                            [&, session](){
                                session->close();
                            }
                        );
                        // clock_gettime(CLOCK_MONOTONIC, &ts[1]);
                        // std::cout << "Initialization Route time: " << (ts[1].tv_sec*1000000 + ts[1].tv_nsec/1000) - (ts[0].tv_sec*1000000 + ts[0].tv_nsec/1000) << std::endl;
                    }
                }
            }
            http::HttpReqRes rr = session->get();
            std::get<http::HttpRequest>(rr) = req;
            session->set(rr);

            // clock_gettime(CLOCK_REALTIME, &ts);
            // std::cout << "controller-app.cpp:1395:routing finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
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

        // clock_gettime(CLOCK_REALTIME, &ts);
        // std::cout << "controller-app.cpp:1404:route_request finished:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << std::endl;
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
                            std::cerr << "controller-app.cpp:1701:JSON parsing failed:" << ec.message() << ":value:" << value << std::endl;
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
        io_mbox_ptr_->sched_signal_ptr->fetch_or(CTL_TERMINATE_EVENT, std::memory_order::memory_order_relaxed);
        io_mbox_ptr_->sched_signal_cv_ptr->notify_one();

        std::unique_lock<std::mutex> lk(*(controller_mbox_ptr_->sched_signal_mtx_ptr));
        controller_mbox_ptr_->sched_signal_cv_ptr->wait(lk);
    }

    Controller::~Controller()
    {
        stop();
        curl_global_cleanup();
    }
}// namespace app
}//namespace controller