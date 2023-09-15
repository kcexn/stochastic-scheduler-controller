#ifndef ECHO_COMMON_HPP
#define ECHO_COMMON_HPP
#include <uuid/uuid.hpp>
#include <transport-servers/unix-server/unix-server.hpp>

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
    enum Signals {
        READ_THREAD = 0x0001,
        UNIX_READ = 0x0002,
        SCTP_WRITE = 0x0004,
        UNIX_WRITE = 0x0008,
        SCHED_START = 0x0010,
        SCHED_END = 0x0020,
        APP_UNIX_WRITE = 0x0040,
        TERMINATE = 0x8000
    };

    // Shared Memory structure to return results from threads.
    struct MailBox
    {
        // Thread Local Signals
        std::mutex mbx_mtx;
        std::condition_variable mbx_cv;
        std::atomic<bool> msg_flag;
        std::atomic<int> signal = 0;

        // Scheduler Signals
        std::shared_ptr<std::mutex> sched_signal_mtx_ptr;
        std::shared_ptr<std::atomic<int> > sched_signal_ptr;
        std::shared_ptr<std::condition_variable> sched_signal_cv_ptr;

        // Payloads
        std::shared_ptr<std::vector<char> > payload_buffer_ptr;
        std::shared_ptr<server::Session> session;
    };
}
#endif