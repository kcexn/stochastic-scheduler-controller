#include "http-session.hpp"
#include <iostream>

namespace http{
    void HttpSession::read()
    {
        acquire_lock();
        HttpRequest& req = std::get<HttpRequest>(*this);
        auto& stream = t_session_->acquire_stream();
        try{
            stream >> req;
            // HttpRequest log_req = req;
            // log_req.http_request_line_complete = false;
            // log_req.next_header = 0;
            // log_req.next_chunk = 0;
            // std::cerr << "http-session.cpp:16:log_req=" << log_req << ",req_next_header=" << req.next_header << ",req_next_chunk=" << req.next_chunk << std::endl;
        } catch (...){
            std::cerr << "http-session.cpp:18:HttpSession.read() failed - current stream value is:" << stream.str() << std::endl;
            throw "what?";
        }
        t_session_->release_stream();
        release_lock();
    }

    void HttpSession::write(const std::function<void()>& fn)
    {
        std::stringstream ss;
        acquire_lock();
        HttpResponse& res = std::get<HttpResponse>(*this);
        // HttpResponse log_res = res;
        // log_res.status_line_finished = false;
        // log_res.next_header = 0;
        // log_res.next_chunk = 0;
        // std::cerr << "http-session.cpp:34:log_res=" << log_res << ",res_next_header=" << res.next_header << ",res_next_chunk=" << res.next_chunk << std::endl;
        ss << res;
        res.status_line_finished = true;
        res.next_header = res.headers.size();
        res.next_chunk = res.chunks.size();
        std::string data_buf(ss.str());
        release_lock();
        t_session_->async_write(
            boost::asio::const_buffer(data_buf.data(), data_buf.size()), 
            fn
        );
    }

    void HttpSession::write(const HttpReqRes& req_res, const std::function<void()>& fn)
    {
        std::stringstream ss;
        ss << std::get<HttpResponse>(req_res);
        std::string data_buf(ss.str());
        // std::cerr << "http-session.cpp:52:log_res=" << data_buf << std::endl;
        t_session_->async_write(
            boost::asio::const_buffer(data_buf.data(), data_buf.size()),
            fn
        );
    }

    void HttpSession::close()
    {
        erase();
    }

    //Http client sessions reverse the http server session logic.
    void HttpClientSession::read()
    {
        acquire_lock();
        HttpResponse& res = std::get<HttpResponse>(*this);
        auto& stream = t_session_->acquire_stream();
        try{
            stream >> res;
            // HttpResponse log_res = res;
            // log_res.status_line_finished = false;
            // log_res.next_header = 0;
            // log_res.next_chunk = 0;
            // std::cerr << "http-session.cpp:76:response=" << log_res << ",res_next_header=" << res.next_header << ",res_next_chunk=" << res.next_chunk << std::endl;
        } catch(...){
            std::cerr << "http-session.cpp:78:HttpClientSession.read() failed - current stream value is: " << stream.str() << std::endl;
            throw "what?";
        }
        t_session_->release_stream();
        release_lock();
        return;
    }

    void HttpClientSession::write(const std::function<void()>& fn)
    {
        std::stringstream ss;
        acquire_lock();
        HttpRequest& req = std::get<HttpRequest>(*this);
        // HttpRequest log_req = req;
        // log_req.http_request_line_complete = false;
        // log_req.next_header = 0;
        // log_req.next_chunk = 0;
        // std::cerr << "http-session.cpp:95:log_req=" << log_req << ",req_next_header=" << req.next_header << ",req_next_chunk=" << req.next_chunk << std::endl;
        ss << req;
        req.http_request_line_complete = true;
        req.next_header = req.headers.size();
        req.next_chunk = req.chunks.size();
        std::string data_buf(ss.str());
        release_lock();
        t_session_->async_write(
            boost::asio::const_buffer(data_buf.data(), data_buf.size()), 
            fn
        );
    }
    
    void HttpClientSession::write(const HttpReqRes& req_res, const std::function<void()>& fn)
    {
        std::stringstream ss;
        ss << std::get<HttpRequest>(req_res);
        std::string data_buf(ss.str());
        // std::cerr << "http-session.cpp:113:log_req=" << data_buf << std::endl;
        t_session_->async_write(
            boost::asio::const_buffer(data_buf.data(), data_buf.size()),
            fn
        );
    }

    void HttpClientSession::close()
    {
        erase();
    }     
}