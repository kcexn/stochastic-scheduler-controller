#include "http-server.hpp"
#include <regex>
#include <boost/json.hpp>
#include <new>
#include "../controller/resources/run/run.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace Http{
    std::regex verb_route("^([A-Z]*) (/.*) ");
    std::regex content_length("Content-Length: ([0-9]*)", std::regex::ECMAScript|std::regex::icase);
    std::regex end_of_headers("^\r$");

    std::istream& operator>>(std::istream& is, Request& req){
        // HTTP is a stream protocol, not a datagram protocol, so 
        // at any given point in time we may have only part of the 
        // http frames. As such, when extracting data from 
        // the HTTP stream, we need to loop until 
        // either all of the available data has been 
        // extracted, or wait until more data arrives.
        // When working with HTTP headers, if the stream
        // is truncated halfway through a header, we must 
        // be able to gracefully recover, and wait for the 
        // remaining headers to arrive. For request bodies,
        // we will simply append to the request body stream 
        // until Content-Length bytes has been reached. 
        // Any trailing data that comes after Content-Length bytes 
        // is simply discarded.
        while ( true ){
            int pos = is.tellg();
            std::smatch matches;

            if ( req.verb.empty() ){
                // If req.verb is empty, that means
                // we have not yet received the first line initializing an http request.

                int bytes_available = is.rdbuf()->in_avail();
                if (bytes_available == 0 ){
                    break;
                }
                std::vector<char> buf(bytes_available);
                std::streamsize len = is.readsome(buf.data(), bytes_available);

                std::string strbuf(buf.data(), len);
                std::string linebuf;
                auto substrpos = strbuf.find("\n", 0);
                if (substrpos == std::string::npos ){
                    // if substrpos == npos
                    // then a new line hasn't been found in the available bytes
                    // of the stream. That means that this line of headers 
                    // has not yet been fully received, so we need 
                    // to reset the stream pointer, and wait for 
                    // more data to arrive.
                    is.seekg(pos);
                    break;
                } else {
                    linebuf.append(strbuf, 0, substrpos);
                    is.seekg(pos+substrpos+1);
                }
                // std::regex verb_route("^([A-Z]*) (/.*) ");
                if (std::regex_search(linebuf, matches, Http::verb_route) ){
                    req.verb.append(matches[1].str());
                    req.route.append(matches[2].str());
                } else {
                    is.setstate(std::ios::failbit);
                }
            } else if ( !req.headers_fully_formed ) {
                int bytes_available = is.rdbuf()->in_avail();
                if (bytes_available == 0 ){
                    break;
                }
                std::vector<char> buf(bytes_available);
                std::streamsize len = is.readsome(buf.data(), bytes_available);

                std::string strbuf(buf.data(), len);
                std::string linebuf;
                auto substrpos = strbuf.find("\n", 0);
                if (substrpos == std::string::npos ){
                    is.seekg(pos);
                    break;
                } else {
                    linebuf.append(strbuf,0,substrpos);
                    is.seekg(pos+substrpos+1);
                }
                // std::regex content_length("Content-Length: ([0-9]*)", std::regex::ECMAScript|std::regex::icase);
                // std::regex end_of_headers("^\r$");

                // For testing only, as it is easier using nc on linux to produce empty lines than carriage returns.
                // std::regex end_of_headers("^$");

                // match for either content-length header or the end of headers marker.
                if(std::regex_search(linebuf, matches, Http::content_length)) {
                    req.content_length = std::stoul(matches[1].str());
                    req.body.reserve(req.content_length);
                }else if(std::regex_search(linebuf, matches, Http::end_of_headers) ){
                    req.headers_fully_formed = true;
                }
            } else if ( !req.body_fully_formed ){
                int buflen = req.content_length - req.body.size();
                char buf[buflen] = {};
                std::streamsize length = is.readsome(buf, buflen);
                req.body.append(buf, length);
                if (req.body.size() == req.content_length ){
                    req.body_fully_formed = true;

                }
                break;
            }
        }
        return is;
    }

    std::vector<Session>& Server::http_sessions(){
        return http_sessions_;
    }

    Session::Session(std::shared_ptr<UnixServer::Session> session)
      : session_ptr_(session),
        request_{}
    {
        #ifdef DEBUG
        std::cout << "HTTP Session Constructor!" << std::endl;
        #endif
    }

    Request Session::read_request(){
        session_ptr_->stream() >> request_;
        return request_;
    }
}//namespace Http
