#include "echo.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

#ifdef DEBUG
#include <iostream>
#endif

echo::app::app(boost::asio::io_context& ioc, short port)
    :  signal_mtx_ptr_(std::make_shared<std::mutex>()),
       signal_ptr_(std::make_shared<std::atomic<int> >()),
       signal_cv_ptr_(std::make_shared<std::condition_variable>()),
       s_ptr_(std::make_shared<sctp_server::server>(ioc,port))
       
{
    #ifdef DEBUG
    std::cout << "App constructor!" << std::endl;
    #endif
    scheduler_();
}

void echo::app::scheduler_(){
    #ifdef DEBUG
    std::cout << "Scheduler Called!" << std::endl;
    #endif

    std::shared_ptr<echo::MailBox> read_mbox_ptr = std::make_shared<echo::MailBox>();
    std::thread read_thread(
        [](
            std::shared_ptr<sctp_server::server> s_ptr_,
            std::shared_ptr<std::mutex> signal_mtx_ptr_,
            std::shared_ptr<echo::MailBox> read_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr_,
            std::shared_ptr<std::condition_variable> signal_cv_ptr_
        )
        {
            #ifdef DEBUG
            std::cout << "read_thread initialized." << std::endl;
            #endif

            // Binary ID for the read thread.
            // Initialize Global Control Information
            const int read_thread_signal = 0x0001;
            std::unique_lock<std::mutex> lk(*signal_mtx_ptr_, std::defer_lock);
            #ifdef DEBUG
            std::cout << "Read Thread Global Controls Initialized." << std::endl;
            #endif

            // Initialize Message Control Information
            std::unique_lock<std::mutex> mbox_lk(read_mbox_ptr->mbx_mtx, std::defer_lock);
            #ifdef DEBUG
            std::cout << "Message Box Controls Initialized." << std::endl;
            #endif

            while((read_mbox_ptr->signal.load() & TERMINATE) != TERMINATE){
                sctp::sctp_message rcvdmsg = s_ptr_->do_read();
                mbox_lk.lock();
                read_mbox_ptr->mbx_cv.wait(mbox_lk, [&]{ return (read_mbox_ptr->msg_flag.load() == false || read_mbox_ptr->signal.load() != 0); });
                read_mbox_ptr->rcvdmsg = rcvdmsg;
                mbox_lk.unlock();
                read_mbox_ptr->msg_flag.store(true);
                signal_ptr_->fetch_or(read_thread_signal, std::memory_order::memory_order_relaxed);
                signal_cv_ptr_->notify_all();
            }
        }, s_ptr_, signal_mtx_ptr_, read_mbox_ptr, signal_ptr_, signal_cv_ptr_
    );
    #ifdef DEBUG
    std::cout << "Reader Started." << std::endl;
    #endif

    std::shared_ptr<echo::MailBox> write_mbox_ptr = std::make_shared<echo::MailBox>();
    std::thread writer_thread(
        [](
            std::shared_ptr<sctp_server::server> s_ptr_,
            std::shared_ptr<std::mutex> signal_mtx_ptr_,
            std::shared_ptr<echo::MailBox> write_mbox_ptr,
            std::shared_ptr<std::atomic<int> > signal_ptr_,
            std::shared_ptr<std::condition_variable> signal_cv_ptr_
        )
        {
            #ifdef DEBUG
            std::cout << "write_thread initialized." << std::endl;
            #endif

            // Initialize Message Control Information
            std::unique_lock<std::mutex> mbox_lk(write_mbox_ptr->mbx_mtx, std::defer_lock);
            #ifdef DEBUG
            std::cout << "Message Box Controls Initialized." << std::endl;
            #endif

            while((write_mbox_ptr->signal.load() & TERMINATE) != TERMINATE){
                mbox_lk.lock();
                write_mbox_ptr->mbx_cv.wait(mbox_lk, [&]{ return (write_mbox_ptr->msg_flag.load() == true || write_mbox_ptr->signal.load() != 0); });
                sctp::sctp_message sndmsg = write_mbox_ptr->sndmsg;
                mbox_lk.unlock();
                write_mbox_ptr->msg_flag.store(false);
                s_ptr_->do_write(sndmsg);
            }
        }, s_ptr_, signal_mtx_ptr_, write_mbox_ptr, signal_ptr_, signal_cv_ptr_
    );
    #ifdef DEBUG
    std::cout << "Writer Thread Started." << std::endl;
    #endif

    // Initialize Global Synchronization Resources.
    std::unique_lock<std::mutex> lk(*signal_mtx_ptr_, std::defer_lock);

    // Start the scheduling loop.
    #ifdef DEBUG
    std::cout << "Scheduling Loop Started." << std::endl;
    #endif

    while(true){
        #ifdef DEBUG
        if (++debug_counter > 4){
            break;
        }
        #endif

        lk.lock();
        signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
        lk.unlock();
        switch(signal_ptr_->load()){
            case READ_THREAD:
                // Read from user.
                read_mbox_ptr->mbx_mtx.lock();
                sctp::sctp_message rcvdmsg = read_mbox_ptr->rcvdmsg;
                read_mbox_ptr->mbx_mtx.unlock();
                // Set Signals
                signal_ptr_->fetch_and(~0x0001, std::memory_order::memory_order_relaxed);
                read_mbox_ptr->msg_flag.store(false);
                read_mbox_ptr->mbx_cv.notify_all();

                //Echo the message back to the user.
                sctp::sctp_message sndmsg = echo_(rcvdmsg);
                write_mbox_ptr->mbx_mtx.lock();
                write_mbox_ptr->sndmsg = sndmsg;
                write_mbox_ptr->mbx_mtx.unlock();
                // Set Signals
                write_mbox_ptr->msg_flag.store(true);
                write_mbox_ptr->mbx_cv.notify_all();
                break;
        }
    }
    // SIGTERM all threads.
    read_mbox_ptr->signal.fetch_or(TERMINATE);
    read_mbox_ptr->mbx_cv.notify_all();

    write_mbox_ptr->signal.fetch_or(TERMINATE);
    write_mbox_ptr->mbx_cv.notify_all();

    read_thread.join();
    writer_thread.join();
}


// Echo needs to be replaced with a router that properly schedules tasks.
sctp::sctp_message echo::app::echo_(sctp::sctp_message& rcvdmsg){
    sctp::sctp_message sndmsg = {
        .rmt_endpt = {
            .endpt = rcvdmsg.rmt_endpt.endpt,
            .rcvinfo = {},
            .sndinfo = {
                .snd_sid = rcvdmsg.rmt_endpt.rcvinfo.rcv_sid,
                .snd_flags = 0,
                .snd_ppid = rcvdmsg.rmt_endpt.rcvinfo.rcv_ppid,
                .snd_context = rcvdmsg.rmt_endpt.rcvinfo.rcv_context,
                .snd_assoc_id = rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id
            }
        },
        .payload = rcvdmsg.payload
    };
    return sndmsg;
}