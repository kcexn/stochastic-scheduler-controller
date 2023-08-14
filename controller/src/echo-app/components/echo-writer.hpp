#ifndef ECHO_WRITER_HPP
#define ECHO_WRITER_HPP
#include "../utils/common.hpp"

namespace echo{
    class EchoWriter
    {
    public:
        EchoWriter(
            std::shared_ptr<sctp_server::server> s_ptr,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<MailBox> write_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );
        void start();
    private:
        std::shared_ptr<sctp_server::server> s_ptr_;
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<MailBox> write_mbox_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;
    };
}
#endif