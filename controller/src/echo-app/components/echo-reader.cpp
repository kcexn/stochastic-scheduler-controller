#include "echo-reader.hpp"

#include <sstream>


#ifdef DEBUG
#include <iostream>
#endif

echo::EchoReader::EchoReader(
    std::shared_ptr<sctp_server::server> s_ptr,
    std::shared_ptr<std::mutex> signal_mtx_ptr,
    std::shared_ptr<echo::MailBox> read_mbox_ptr,
    std::shared_ptr<std::atomic<int> > signal_ptr,
    std::shared_ptr<std::condition_variable> signal_cv_ptr
) : s_ptr_(s_ptr),
    signal_mtx_ptr_(signal_mtx_ptr),
    read_mbox_ptr_(read_mbox_ptr),
    signal_ptr_(signal_ptr),
    signal_cv_ptr_(signal_cv_ptr)
{
    #ifdef DEBUG
    std::cout<< "Echo Reader Constructor!" << std::endl;
    #endif
}

#ifdef DEBUG
echo::EchoReader::~EchoReader(){
    std::cout << "Echo Reader Destructor!" << std::endl;
}
#endif

void echo::EchoReader::start(){
    #ifdef DEBUG
    std::cout << "read_thread initialized." << std::endl;
    #endif

    // Open an SCTP Endpoint.
    async_sctp_read();
    s_ptr_->start();

    #ifdef DEBUG
    std::cout << "Reader Terminated." << std::endl;
    #endif
    pthread_exit(0);
}

void echo::EchoReader::async_sctp_read(){
    s_ptr_->async_read(
        [&] (const boost::system::error_code& ec){
            if(!ec){
                sctp::sctp_message rcvdmsg = s_ptr_->do_read();
                std::unique_lock<std::mutex> mbox_lk(read_mbox_ptr_->mbx_mtx);
                read_mbox_ptr_->mbx_cv.wait(mbox_lk, [&]{ return (read_mbox_ptr_->msg_flag.load() == false || read_mbox_ptr_->signal.load() != 0); });
                read_mbox_ptr_->rcvdmsg = rcvdmsg;
                mbox_lk.unlock();
                if( (read_mbox_ptr_->signal.load() & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                    pthread_exit(0);
                }
                read_mbox_ptr_->msg_flag.store(true);
                signal_ptr_->fetch_or(echo::Signals::READ_THREAD, std::memory_order::memory_order_relaxed);
                signal_cv_ptr_->notify_all();
                async_sctp_read();
            }
        }
    );
}