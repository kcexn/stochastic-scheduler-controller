#ifndef CONTROLLER_IO_HPP
#define CONTROLLER_IO_HPP
#include <memory>
#include <deque>
#include <transport-servers/sctp-server/sctp-server.hpp>
#include <transport-servers/unix-server/unix-server.hpp>
#include <sys/eventfd.h>
/*Forward Declarations*/
namespace boost{
namespace asio{
    class io_context;
}
}

namespace libcurl{
    class CurlMultiHandle;
}

namespace controller{
namespace io{
    struct MessageBox{
        // Child Thread Signals
        std::mutex mbx_mtx;
        std::condition_variable mbx_cv;
        std::atomic<std::uint16_t> signal;

        // Shared Flag.
        std::atomic<bool> msg_flag;

        // Parent Thread Signals
        std::shared_ptr<std::mutex> sched_signal_mtx_ptr;
        std::shared_ptr<std::atomic<std::uint16_t> > sched_signal_ptr;
        std::shared_ptr<std::condition_variable> sched_signal_cv_ptr;

        // Payload.
        std::shared_ptr<server::Session> session;

        // eventfd
        int efd;

        // Explicit default constructor.
        explicit MessageBox():
            sched_signal_mtx_ptr(std::make_shared<std::mutex>()),
            sched_signal_ptr(std::make_shared<std::atomic<std::uint16_t> >()),
            sched_signal_cv_ptr(std::make_shared<std::condition_variable>())
        {}
    };

    typedef std::deque<std::shared_ptr<MessageBox> > MessageQueue;

    // This class encapsulates all of the io operations we need for the 
    // controller
    class IO
    {
    public:
        static constexpr std::size_t MAX_QUEUE_LENGTH = 1024;
        IO(
            std::shared_ptr<MessageBox> mbox, 
            const std::string& local_endpoint, 
            boost::asio::io_context& ioc, 
            int* num_running_multi_handles, 
            std::shared_ptr<libcurl::CurlMultiHandle> cmhp
        );
        IO(
            std::shared_ptr<MessageBox> mbox, 
            const std::string& local_endpoint, 
            boost::asio::io_context& ioc, 
            std::uint16_t sport, 
            int* num_running_multi_handles, 
            std::shared_ptr<libcurl::CurlMultiHandle> cmhp
        );
        void start();
        void stop();

        bool mq_is_empty() { std::unique_lock<std::mutex> lk(mq_mtx_); return mq_.empty(); }
        bool mq_is_full() { std::unique_lock<std::mutex> lk(mq_mtx_); return (mq_.size() >= MAX_QUEUE_LENGTH); }
        std::shared_ptr<MessageBox> mq_pull(){ 
            std::unique_lock<std::mutex> lk(mq_mtx_);
            if(!mq_.empty()){
                auto head = mq_.front();
                mq_.pop_front();
                return head;
            } else {
                return std::shared_ptr<MessageBox>();
            }
        }
        void mq_push(const std::shared_ptr<MessageBox>& msg) { std::unique_lock<std::mutex> lk(mq_mtx_); mq_.push_back(msg); return; }

        /* Async Connect routes the connection request based on the address information in server::Remote */
        void async_connect(server::Remote, std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)>);

        server::Remote local_sctp_address;

        ~IO();
    private:
        std::shared_ptr<MessageBox> mbox_ptr_;
        pthread_t io_;
        boost::asio::io_context& ioc_;

        // SCTP Socket Server
        sctp_transport::SctpServer ss_;
        
        // Unix Socket Server
        UnixServer::unix_server us_;

        int* num_running_multi_handles_;
        std::shared_ptr<libcurl::CurlMultiHandle> cmhp_;

        std::atomic<bool> stopped_;
        std::mutex stop_;
        std::condition_variable stop_cv_;

        std::mutex mq_mtx_;
        MessageQueue mq_;
    };
}// namespace io
}// namespace controller
#endif