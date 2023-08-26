#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "../unix-server/unix-server.hpp"
#include <memory>
#include <istream>
#include <string>
#include <vector>

#ifdef DEBUG
#include <iostream>
#endif

namespace Http{
    struct Request {
        std::string verb;
        std::string route;
        std::size_t content_length;
        std::string body;
        bool headers_fully_formed;
        bool body_fully_formed;
    };

    inline bool operator==(const Request& lhs, const Request& rhs)
    {
        return (
            lhs.verb == rhs.verb &&
            lhs.route == rhs.route &&
            lhs.content_length == rhs.content_length &&
            lhs.body == rhs.body
        );
    }

    struct Response {
        std::string status_code;
        std::string status_message;
        std::string connection;
        std::size_t content_length;
        std::string body;
    };

    std::istream& operator>>(std::istream& is, Request& req);

    class Session{
    public:
        Session(std::shared_ptr<UnixServer::Session> session);
        Request read_request();
        Request& request() { return request_; }
        boost::asio::local::stream_protocol::socket& socket(){ return session_ptr_->socket(); }
        std::shared_ptr<UnixServer::Session>& unix_session(){ return session_ptr_; }

        #ifdef DEBUG
        ~Session(){
            std::cout << "HTTP Session Destructor!" << std::endl;
        }
        #endif
        bool operator==(const Session& other) const {
            return session_ptr_ == other.session_ptr_;
        }

    private:
        std::shared_ptr<UnixServer::Session> session_ptr_;
        Request request_;
    };

    class Server
    {
    public:
        Server()
        {
            #ifdef DEBUG
            std::cout << "HTTP Server Constructor!" << std::endl;
            #endif
        }
        std::vector<Session>& http_sessions();

        #ifdef DEBUG
        ~Server(){
            std::cout << "HTTP Server Destructor!" << std::endl;
        }
        #endif
    private:
        std::vector<Session> http_sessions_;
    };
}// Namespace HttpServer
#endif