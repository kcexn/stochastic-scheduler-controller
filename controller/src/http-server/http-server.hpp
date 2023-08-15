#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "../unix-server/unix-server.hpp"
#include <memory>
#include <sstream>

namespace HttpServer{
    class Session{
    public:
        Session(std::shared_ptr<UnixServer::Session> session): session_ptr_(session){}


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

    private:
        std::basic_stringstream request_;
        std::basic_stringstream response_;
        std::shared_ptr<UnixServer::Session> session_ptr_;
    }

    class Server{
    public:
    private:
    }
}// Namespace HttpServer
#endif