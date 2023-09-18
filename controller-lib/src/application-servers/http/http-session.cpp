#include "http-session.hpp"

namespace http{
    void HttpSession::read()
    {
        acquire_lock();
        HttpRequest& req = std::get<HttpRequest>(*this);
        t_session_->acquire_stream() >> req;
        t_session_->release_stream();
        release_lock();
    }

    void HttpSession::write(const std::function<void()>& fn)
    {
        std::stringstream ss;
        acquire_lock();
        HttpResponse& res = std::get<HttpResponse>(*this);
        ss << res;
        res.status_line_finished = true;
        res.next_header = res.headers.size();
        res.next_chunk = res.chunks.size();
        release_lock();
        std::string data_buf(ss.str());
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
        t_session_->acquire_stream() >> res;
        t_session_->release_stream();
        release_lock();
        return;
    }

    void HttpClientSession::write(const std::function<void()>& fn)
    {
        std::stringstream ss;
        acquire_lock();
        HttpRequest& req = std::get<HttpRequest>(*this);
        ss << req;
        req.http_request_line_complete = true;
        req.next_header = req.headers.size();
        req.next_chunk = req.chunks.size();
        release_lock();
        std::string data_buf(ss.str());
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