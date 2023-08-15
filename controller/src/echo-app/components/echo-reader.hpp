#ifndef ECHO_READER_HPP
#define ECHO_READER_HPP
#include "../utils/common.hpp"
#include "../../unix-server/unix-server.hpp"

namespace echo{   
    class EchoReader
    {
    public:
        EchoReader(
            std::shared_ptr<sctp_server::server> s_ptr,
            std::shared_ptr<UnixServer::Server> us_ptr,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<MailBox> read_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );
        #ifdef DEBUG
        ~EchoReader();
        #endif
        void start();
        void async_sctp_read();
        void async_unix_read(std::shared_ptr<UnixServer::Session>& session);
        void async_unix_accept();
        std::vector<std::shared_ptr<UnixServer::Session> > sessions(){ return unix_session_ptrs_; }
    private:
        std::shared_ptr<sctp_server::server> s_ptr_;
        std::shared_ptr<UnixServer::Server> us_ptr_;
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<MailBox> read_mbox_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;
        std::vector<std::shared_ptr<UnixServer::Session> > unix_session_ptrs_;
    };
}//namespace echo.

#endif