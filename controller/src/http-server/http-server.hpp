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
        std::string location;
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

        // The only headers I plan to parse correctly.
        // Everything else should be completely ignored.
        // I'll forward proxy everything through NGINX and additional HTTP headers that are required 
        // can be added by NGINX. This includes TLS if needed.

        // The routes I need to handle correctly are:
        //
        // POST /init HTTP/1.1
        // Content-Type: application/json
        // Content-Length: 1*DIGIT
        // 
        // {
        //   "value": {
        //     "name" : String,
        //     "main" : String,
        //     "code" : String,
        //     "binary": Boolean,
        //     "env": Map[String, String]
        //   }
        // }
        
        // With a response:
        // HTTP/1.1 200 OK
        // Content-Type: application/json
        // Content-Length: 0
        //

        // OR
        // HTTP/1.1 404 Not Found
        // Content-Type: applicaton/json
        // Content-Length: 1*DIGIT
        //
        // { "error": "{ERROR}" }

        // POST /run HTTP/1.1
        // Content-Type: application/json
        // Content-Length: 1*DIGIT
        // 
        // {
        //   "value": JSON,
        //   "namespace": String,
        //   "action_name": String,
        //   "api_host": String,
        //   "api_key": String,
        //   "activation_id": String,
        //   "transaction_id": String,
        //   "deadline": Number
        // }      

        // The activation record that I need to be able to respond with meets the following schema
        //
        // HTTP/1.1 200 OK
        // Content-Type: application/json
        // Content-Length: 1*DIGIT
        // 
        // {
        //   "activationId": "{activation_id}",
        //   "namespace": "{namespace}",
        //   "name": "{name}",
        //   "start": "{start-time}", /* In Unix Time (seconds since Unix Epoch) */
        //   "end": "{end-time}",
        //   "logs" [
        //      "{TIMESTAMP} {STREAM}: {LOG LINE}", ... XXX_THE_END_OF_A_WHISK_ACTIVATION_XXX/* TIMESTAMP should be given in ISO TIME, STREAM is either stdout or stderr, and LOG LINE corresponds to the stream output */
        //    ],
        //   "annotations": [
        //      {"key": "value"}, ... /* These are special values used by openwhisk for certain types of special actions. */
        //   ],
        //   "response": {
        //      "status": "{success | application error | action developer error | whisk internal error}"
        //      "statusCode": {0 | 1 | 2 | 3},
        //      "success": {true | false},
        //      "result": {
        //          "execution-context-id": "{UUID}",
        //          "payload": {
        //              ....
        //          },
        //          "error": "{ERROR}"
        //      }
        //   }
        // }


        // I need to be able to generate a request to:
        // Given that this is a fairly simple requirement, and will be done ONCE at the beginning of 
        // each new execution context, I might just cheat and fork-execve this out to a cURL client.
        //
        // POST /namespaces/{namespace}/actions/{actionName} HTTP/1.1
        // Content-Type: application/json
        // Content-Length: 1*DIGIT
        //
        // {
        //   "execution-context-id": "{UUID}",
        //   "peers": [
        //      "HOST:PORT",
        //      "HOST:PORT", ...
        //   ],
        //   "payload": {
        //      ...
        //   }
        // }
        //

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