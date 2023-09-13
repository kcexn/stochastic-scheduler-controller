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
            ss << std::get<HttpResponse>(*this);
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
}