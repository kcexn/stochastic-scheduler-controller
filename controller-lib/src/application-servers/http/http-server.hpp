#ifndef OWLIB_HTTP_SERVER_HPP
#define OWLIB_HTTP_SERVER_HPP
#include "../server/app-server.hpp"
#include "http-requests.hpp"

namespace http{
    typedef app_server::Server<HttpRequest, HttpResponse> http_server;
    class HttpServer: public http_server
    {
    public:
        ~HttpServer() override = default;
    private:
    };

}// namespace http
#endif