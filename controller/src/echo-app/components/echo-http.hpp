#ifndef ECHO_HTTP_HPP
#define ECHO_HTTP_HPP
#include "../utils/common.hpp"
#include "../../http-server/http-server.hpp"
#include <thread>
#include <memory>

#ifdef DEBUG
#include <iostream>
#endif

namespace echo{
    class HttpServer
    {
    public:
        HttpServer(std::shared_ptr<MailBox> mbox_ptr);
        void start();
        void stop();
        ~HttpServer();

    private:
        std::shared_ptr<MailBox> http_mbox_ptr_;
        Http::Server http_server_;
        pthread_t tid_;
    };
}//Namespace Echo
#endif