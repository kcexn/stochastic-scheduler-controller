#ifndef ECHO_READER_HPP
#define ECHO_READER_HPP
#include "../utils/common.hpp"

namespace echo{   
    class EchoReader
    {
    public:
        EchoReader(
            std::shared_ptr<sctp_server::server> s_ptr,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<MailBox> read_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );
        void start();
        void async_sctp_read();
    private:
        std::shared_ptr<sctp_server::server> s_ptr_;
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<MailBox> read_mbox_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;
    };
}//namespace echo.

#endif