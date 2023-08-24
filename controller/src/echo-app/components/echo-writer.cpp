#include "echo-writer.hpp"
#ifdef DEBUG
#include <iostream>
#endif

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

#ifdef DEBUG
echo::EchoWriter::~EchoWriter(){
    std::cout << "Echo Writer Destructor!" << std::endl;
}
#endif

void echo::EchoWriter::start(){
    #ifdef DEBUG
    std::cout << "write_thread initialized." << std::endl;
    #endif

    // Initialize Message Control Information
    std::unique_lock<std::mutex> mbox_lk(write_mbox_ptr_->mbx_mtx, std::defer_lock);
    #ifdef DEBUG
    std::cout << "Message Box Controls Initialized." << std::endl;
    #endif

    while((write_mbox_ptr_->signal.load() & echo::Signals::TERMINATE) != echo::Signals::TERMINATE){
        mbox_lk.lock();
        write_mbox_ptr_->mbx_cv.wait(mbox_lk, [&]{ return (write_mbox_ptr_->msg_flag.load() == true || write_mbox_ptr_->signal.load() != 0); });
        mbox_lk.unlock();
        std::atomic<int> signal = write_mbox_ptr_->signal.load();
        if ( (signal & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ) {
            // If TERMINATE signal received. Exit.
            #ifdef DEBUG
            std::cout << "Writer Thread Closing" << std::endl;
            #endif
            pthread_exit(0);
        }
        if ((signal & echo::Signals::UNIX_WRITE) == echo::Signals::UNIX_WRITE){
            mbox_lk.lock();
            std::shared_ptr<UnixServer::Session> session_ptr(write_mbox_ptr_->session_ptr);
            mbox_lk.unlock();
            
            // Unset the write signals.
            write_mbox_ptr_->signal.fetch_and(~echo::Signals::UNIX_WRITE, std::memory_order::memory_order_relaxed);
            write_mbox_ptr_->msg_flag.store(false);
            write_mbox_ptr_->mbx_cv.notify_all();

            session_ptr->do_write(session_ptr->buflen());
            
        } else if ((signal & echo::Signals::SCTP_WRITE) == echo::Signals::SCTP_WRITE){
            mbox_lk.lock();
            sctp::sctp_message sndmsg(write_mbox_ptr_->sndmsg);
            mbox_lk.unlock();

            //Unset the write signals.
            write_mbox_ptr_->signal.fetch_and(~echo::Signals::SCTP_WRITE, std::memory_order::memory_order_relaxed);
            write_mbox_ptr_->msg_flag.store(false);
            write_mbox_ptr_->mbx_cv.notify_all();

            s_ptr_->do_write(sndmsg);

        } else if ((signal & echo::Signals::APP_UNIX_WRITE) == echo::Signals::APP_UNIX_WRITE){
            mbox_lk.lock();
            std::string str(write_mbox_ptr_->payload_buffer_ptr->begin(), write_mbox_ptr_->payload_buffer_ptr->end());
            std::shared_ptr<UnixServer::Session> session_ptr(write_mbox_ptr_->session_ptr);
            mbox_lk.unlock();

            //clear signals.
            write_mbox_ptr_->signal.fetch_and(~echo::Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
            write_mbox_ptr_->msg_flag.store(false);
            write_mbox_ptr_->mbx_cv.notify_all();

            // Write on the output of the socket.
            session_ptr->socket().write_some(boost::asio::buffer(str.data(), str.size()));
            session_ptr->cancel();
            session_ptr->socket().shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
            session_ptr->socket().close();
        }
        //Unset the common write signals.
        write_mbox_ptr_->msg_flag.store(false);
        write_mbox_ptr_->mbx_cv.notify_all();
    }

    #ifdef DEBUG
    std::cout << "Writer Thread Closing" << std::endl;
    #endif
    pthread_exit(0);
}