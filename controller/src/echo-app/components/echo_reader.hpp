#ifndef ECHO_READER_HPP
#define ECHO_READER_HPP
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "../../sctp-server/sctp-server.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace echo{
    class EchoReader
    {
    public:
        EchoReader(
            std::shared_ptr<sctp_server::server> s_ptr,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<echo::MailBox> read_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );
        void start();
    private:
        std::shared_ptr<sctp_server::server> s_ptr_;
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<echo::MailBox> read_mbox_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;
    };

}//namespace echo

#endif