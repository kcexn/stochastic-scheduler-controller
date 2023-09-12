#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP
#include "../server/app-session.hpp"
#include "http-requests.hpp"
#include "http-server.hpp"

namespace http{
    typedef app_server::Session<HttpRequest, HttpResponse> http_session;

    //Http Sessions Contain a single request, and a single response.
    class HttpSession: public http_session
    {
    public:
        HttpSession(HttpServer& server): http_session(server) {}
        HttpSession(HttpServer& server, const std::shared_ptr<server::Session>& t_session_ptr): http_session(server, t_session_ptr) {}

        void read() override;
        void write(const std::function<void()>& fn) override;
        void close() override;
    };
}
#endif