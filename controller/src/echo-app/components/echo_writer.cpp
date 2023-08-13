#include "../echo.hpp"
#include "echo_writer.hpp"

echo::EchoWriter::EchoWriter(
    std::shared_ptr<sctp_server::server> s_ptr,
    std::shared_ptr<std::mutex> signal_mtx_ptr,
    std::shared_ptr<echo::MailBox> write_mbox_ptr,
    std::shared_ptr<std::atomic<int> > signal_ptr,
    std::shared_ptr<std::condition_variable> signal_cv_ptr
) : s_ptr_(s_ptr),
    signal_mtx_ptr_(signal_mtx_ptr),
    write_mbox_ptr_(write_mbox_ptr),
    signal_ptr_(signal_ptr),
    signal_cv_ptr_(signal_cv_ptr)
{
    #ifdef DEBUG
    std::cout<< "Echo Writer Constructor!" << std::endl;
    #endif
}
void echo::EchoWriter::start(){
    #ifdef DEBUG
    std::cout << "write_thread initialized." << std::endl;
    #endif

    // Initialize Message Control Information
    std::unique_lock<std::mutex> mbox_lk(write_mbox_ptr_->mbx_mtx, std::defer_lock);
    #ifdef DEBUG
    std::cout << "Message Box Controls Initialized." << std::endl;
    #endif

    while((write_mbox_ptr_->signal.load() & echo::TERMINATE) != echo::TERMINATE){
        mbox_lk.lock();
        write_mbox_ptr_->mbx_cv.wait(mbox_lk, [&]{ return (write_mbox_ptr_->msg_flag.load() == true || write_mbox_ptr_->signal.load() != 0); });
        sctp::sctp_message sndmsg = write_mbox_ptr_->sndmsg;
        mbox_lk.unlock();
        if ( (write_mbox_ptr_->signal.load() & echo::TERMINATE) == echo::TERMINATE){
            // If TERMINATE signal received. Exit.
            #ifdef DEBUG
            std::cout << "Writer Thread Closing" << std::endl;
            #endif
            pthread_exit(0);
        }
        write_mbox_ptr_->msg_flag.store(false);
        s_ptr_->do_write(sndmsg);
    }

    #ifdef DEBUG
    std::cout << "Writer Thread Closing" << std::endl;
    #endif
    pthread_exit(0);
}