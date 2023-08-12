#include <thread>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <list>

#include <boost/asio.hpp>

#include "sctp.hpp"
#include "sctp-server.hpp"

namespace echo{

    // Enum identifying signal source/destination. 
    // TERMINATE signals are a lightweight asynchronous
    // method to notify threads that they should terminate.
    // Unlike pthread_cancel, or pthread_kill, these 
    // application signals do not wake up sleeping or blocked 
    // threads. Instead, they notify the thread that it should eventually
    // clean up itself. Immediate terminations should be handled 
    // using OS system calls such as pthread_cancel or pthread_kill 
    // rather than application message box signals.
    enum{
        READ_THREAD = 0x0001,
        WRITE_THREAD = 0x0002,
        TERMINATE = 0x8000,
    };

    // Shared Memory structure to return results from threads.
    struct MailBox
    {
        std::mutex mbx_mtx;
        std::condition_variable mbx_cv;
        std::atomic<bool> msg_flag;
        std::atomic<int> signal = 0;
        sctp::sctp_message rcvdmsg = {};
        sctp::sctp_message sndmsg = {};
    };

    class app: public std::enable_shared_from_this<app>
    {
    public:
        app(boost::asio::io_context& ioc, short port);

    private:
        #ifdef DEBUG
        int debug_counter = 0;
        #endif

        // Global Scheduling State Variables
        std::shared_ptr<std::mutex> signal_mtx_ptr_;
        std::shared_ptr<std::atomic<int> > signal_ptr_;
        std::shared_ptr<std::condition_variable> signal_cv_ptr_;

        // Application Components
        void reader_thread_fn_(
            std::shared_ptr<sctp_server::server> s_ptr_,
            std::shared_ptr<std::mutex> signal_mtx_ptr_,
            std::shared_ptr<echo::MailBox> read_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr_,
            std::shared_ptr<std::condition_variable> signal_cv_ptr_
        );
        void writer_thread_fn_(
            std::shared_ptr<sctp_server::server> s_ptr_,
            std::shared_ptr<std::mutex> signal_mtx_ptr_,
            std::shared_ptr<echo::MailBox> write_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr_,
            std::shared_ptr<std::condition_variable> signal_cv_ptr_
        );
        void scheduler_();
        std::shared_ptr<sctp_server::server> s_ptr_;
        
        // Echo Application Scheduled.
        sctp::sctp_message echo_(sctp::sctp_message& rcvdmsg);

        // Shared Memory Structure to Track Return Values from multiple threads.
        std::vector<std::shared_ptr<MailBox> > results;
        std::vector<sctp_server::sctp_stream> stream_table;
        std::vector<std::thread> thread_table;
    };
}//echo namespace