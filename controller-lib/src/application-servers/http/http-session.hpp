#ifndef OWLIB_HTTP_SESSION_HPP
#define OWLIB_HTTP_SESSION_HPP
#include "http-requests.hpp"
#include "../server/app-session.hpp"
#include "http-server.hpp"

namespace http{
    typedef std::tuple<HttpRequest, HttpResponse> HttpReqRes;
    typedef app_server::Session<HttpRequest, HttpResponse> http_session;

    //Http Sessions Contain a single request, and a single response.
    class HttpSession: public http_session
    {
    public:
        HttpSession(HttpServer& server): http_session(server) {}
        HttpSession(HttpServer& server, const std::shared_ptr<server::Session>& t_session_ptr): http_session(server, t_session_ptr) {}

        void read() override;
        void write(const std::function<void()>& fn) override;
        void write(const HttpReqRes& req_res, const std::function<void()>& fn) override;
        void close() override;

        ~HttpSession() {             
            if(t_session_){
                t_session_->erase();
            }
        }
    };

    //Http client sessions reverse the http server session logic.
    class HttpClientSession: public http_session
    {
    public:
        HttpClientSession(HttpServer& server): http_session(server) {}
        HttpClientSession(HttpServer& server, const std::shared_ptr<server::Session>& t_session_ptr): http_session(server, t_session_ptr) {}

        void read() override;
        void write(const std::function<void()>& fn) override;
        void write(const HttpReqRes& req_res, const std::function<void()>& fn) override;
        void close() override;

        ~HttpClientSession() {             
            if(t_session_){
                t_session_->erase();
            }
        }        
    };
}
#endif