#ifndef ECHO_COMMON_HPP
#define ECHO_COMMON_HPP
#include <mutex>
#include <condition_variable>
#include "../../utils/uuid.hpp"
#include "../../unix-server/unix-server.hpp"
#include "../../sctp-server/sctp-server.hpp"

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
        sctp::sctp_message rcvdmsg = {};
        sctp::sctp_message sndmsg = {};
        std::shared_ptr<std::vector<char> > payload_buffer_ptr;
        std::shared_ptr<UnixServer::Session> session_ptr;
    };

    class ExecutionContext
    {
    public:
        ExecutionContext(const sctp::assoc_t& assoc_id, const sctp::sid_t& sid)
          : assoc_id_{assoc_id},
            sid_{sid},
            execution_context_(UUID::uuid_create_v4()) 
        {
            if ((execution_context_.time_hi_and_version & UUID::UUID_V4) != UUID::UUID_V4){
                throw;
            }
            #ifdef DEBUG
            std::cout << execution_context_ << std::endl;
            #endif
        }
        sctp::assoc_t assoc_id() { return assoc_id_; }
        sctp::sid_t sid() { return sid_; }
        void set_tid(pthread_t t_id) {
            tid_ = {
                .id_ = t_id,
                .tid_set_ = true
            };
        }
        pthread_t get_tid(){
            if(tid_.tid_set_) {
                return tid_.id_;
            } else {
                return 0;
            }
        }
        inline bool operator==(const ExecutionContext& other) { return (assoc_id_ == other.assoc_id_ && sid_ == other.sid_); }
    private:
        sctp::assoc_t assoc_id_;
        sctp::sid_t sid_;
        UUID::uuid_t execution_context_;

        // TIDs should be considered opaque blocks of memory (not portable) and so
        // equality checks should be made ONLY if we are sure that the tid
        // has been set. Equality checks should only be done with man(3) pthread_equal().
        struct tid_t {
            pthread_t id_;
            bool tid_set_ = false;
        } tid_;
    };
}
#endif