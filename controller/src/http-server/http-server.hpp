#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "transport-servers/unix-server/unix-server.hpp"
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
    // This HTTP::Session is the OSI Layer 6 Presentation
    // Layer. All of the logic to serialize and deserialize HTTP should be 
    // stored in these HTTP::Sessions.
    public:
        // Session(const std::shared_ptr<UnixServer::Session>& session_ptr);
        Session(const std::shared_ptr<server::Session>& session_ptr);


        const std::shared_ptr<Request>& read_request();
        std::shared_ptr<Request>& request() { return request_; }
        std::shared_ptr<server::Session>& session(){ return session_ptr_; }


        #ifdef DEBUG
        ~Session(){
            std::cout << "HTTP Session Destructor!" << std::endl;
        }
        #endif
        bool operator==(const Session& other) const {
            return session_ptr_ == other.session_ptr_;
        }

    private:
        std::shared_ptr<server::Session> session_ptr_;
        std::shared_ptr<Request> request_;
        std::shared_ptr<Response> response_;
    };

    class Server
    {
    // An HTTP Server is a container for HTTP Sessions, that implements many of the elements of a std::vector.
    public:
        Server()
        {
            #ifdef DEBUG
            std::cout << "HTTP Server Constructor!" << std::endl;
            #endif
        }

        std::vector<Session>& http_sessions();
        // Re-export iterator functions in std::vector so that Http::Server implements
        // an iterator interface.
        std::vector<Session>::iterator begin() { return http_sessions_.begin(); }
        std::vector<Session>::const_iterator cbegin() const { return http_sessions_.cbegin(); }
        std::vector<Session>::iterator end() { return http_sessions_.end(); }
        std::vector<Session>::const_iterator cend() const { return http_sessions_.cend(); }
        std::vector<Session>::reverse_iterator rbegin() { return http_sessions_.rbegin(); }
        std::vector<Session>::const_reverse_iterator crbegin() const { return http_sessions_.crbegin(); }
        std::vector<Session>::reverse_iterator rend() { return http_sessions_.rend(); }
        std::vector<Session>::const_reverse_iterator crend() const { return http_sessions_.crend(); }
        bool empty() { return http_sessions_.empty(); }
        std::vector<Session>::size_type size() { return http_sessions_.size(); }
        void clear() { http_sessions_.clear(); }
        void erase( const std::vector<Session>::iterator& it ) { http_sessions_.erase(it); }
        void erase ( const std::vector<Session>::const_iterator& it ){ http_sessions_.erase(it); }
        void push_back(const Session& http_session) { http_sessions_.push_back(http_session); }
        void push_back(Session&& http_session) { http_sessions_.push_back(std::move(http_session)); }
        Session& back() { return http_sessions_.back(); }
        const Session& back() const { return http_sessions_.back(); }

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