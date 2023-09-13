#ifndef ECHO_SCHEDULER_HPP
#define ECHO_SCHEDULER_HPP
#include "echo-reader.hpp"
#include "echo-writer.hpp"
#include "echo-application.hpp"
/*Forward Declarations*/
namespace controller{
namespace app{
    class Controller;
}
}

namespace echo {
    class Scheduler : public std::enable_shared_from_this<Scheduler>
    {
    public:
        Scheduler(
            boost::asio::io_context& ioc, 
            short port,
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );
        #ifdef DEBUG
        ~Scheduler();
        #endif
        void start();
        void run();
    private:
        // Servers
        std::shared_ptr<sctp_server::server> s_ptr_;

        // // Scheduler Signals.
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;

        // Reader and Writer Threads.
        std::shared_ptr<MailBox> read_mbox_ptr_;
        std::shared_ptr<MailBox> write_mbox_ptr_;
        EchoReader echo_reader_;
        EchoWriter echo_writer_;

        // Controller Application
        std::shared_ptr<MailBox> controller_mbox_ptr_;
        std::shared_ptr<controller::app::Controller> controller_ptr_;

        // Echo Application.
        App app_;
        std::vector<std::shared_ptr<MailBox> > results_;
        std::vector<echo::ExecutionContext> context_table_;

        boost::asio::io_context& ioc_;

        pthread_t reader_;
        pthread_t writer_;
    };
}//namespace echo.
#endif