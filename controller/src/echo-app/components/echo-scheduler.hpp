#ifndef ECHO_SCHEDULER_HPP
#define ECHO_SCHEDULER_HPP
#include "../utils/common.hpp"
#include "echo-reader.hpp"
#include "echo-writer.hpp"
#include "echo-application.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace echo {
    class Scheduler : public std::enable_shared_from_this<Scheduler>
    {
    public:
        Scheduler(boost::asio::io_context& ioc, short port);
        void start();
    private:
        // SCTP Server.
        std::shared_ptr<sctp_server::server> s_ptr_;

        // HTTP Server.
        // HTTP-TCP Server.
        // MsgBox Holding the TCP Session.
        // void http-server->DoAccept();

        // Scheduler Signals.
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;

        // Reader and Writer Threads.
        std::shared_ptr<MailBox> read_mbox_ptr_;
        std::shared_ptr<MailBox> write_mbox_ptr_;
        echo::EchoReader echo_reader_;
        echo::EchoWriter echo_writer_;

        // Echo Application.
        echo::App app_;
        std::vector<std::shared_ptr<MailBox> > results_;
        std::vector<echo::ExecutionContext> context_table_;

        boost::asio::io_context& ioc_;
    };
}//namespace echo.
#endif