#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "../server/app-server.hpp"
#include "http-requests.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace http{
    typedef app_server::Server<HttpRequest, HttpResponse> http_server;
    class HttpServer: public http_server{};

}// namespace http
#endif