#ifndef ECHO_SCHEDULER_HPP
#define ECHO_SCHEDULER_HPP
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
/*Forward Declarations*/
namespace boost{
namespace asio{
    class io_context;
}
}


namespace controller{
namespace app{
    class Controller;
}
}

namespace echo {
    struct MailBox; 

    class Scheduler : public std::enable_shared_from_this<Scheduler>
    {
    public:
        Scheduler(
            boost::asio::io_context& ioc, 
            std::shared_ptr<std::mutex> signal_mtx_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr,
            std::shared_ptr<std::condition_variable> signal_cv_ptr
        );

        void start();

        #ifdef DEBUG
        ~Scheduler();
        #endif
    private:
        // Controller Application
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;
        std::shared_ptr<echo::MailBox> controller_mbox_ptr_;
        std::shared_ptr<controller::app::Controller> controller_ptr_;
        boost::asio::io_context& ioc_;
    };
}//namespace echo.
#endif