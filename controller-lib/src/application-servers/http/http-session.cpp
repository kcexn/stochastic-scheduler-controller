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
            boost::asio::const_buffer buf(data_buf.data(), data_buf.size());
            t_session_->async_write(buf, fn);
        }

        void HttpSession::close()
        {
            erase();
        }
}