#include "echo-scheduler.hpp"

echo::Scheduler::Scheduler(boost::asio::io_context& ioc, short port)
    : s_ptr_(std::make_shared<sctp_server::server>(ioc, port)),
    signal_mtx_ptr_(std::make_shared<std::mutex>()),
    signal_ptr_(std::make_shared<std::atomic<int >>()),
    signal_cv_ptr_(std::make_shared<std::condition_variable>()),
    read_mbox_ptr_(std::make_shared<MailBox>()),
    write_mbox_ptr_(std::make_shared<MailBox>()),
    echo_reader_(
        s_ptr_,
        signal_mtx_ptr_,
        read_mbox_ptr_,
        signal_ptr_,
        signal_cv_ptr_
    ),
    echo_writer_(
        s_ptr_,
        signal_mtx_ptr_,
        write_mbox_ptr_,
        signal_ptr_,
        signal_cv_ptr_                
    ),
    app_(
        results_,
        context_table_,
        s_ptr_
    ), 
    ioc_(ioc) {}

void echo::Scheduler::start(){
    #ifdef DEBUG
    int debug_counter = 0;
    std::cout << "Scheduler Called!" << std::endl;
    #endif

    std::thread read_thread(
        &echo::EchoReader::start, &echo_reader_
    );
    #ifdef DEBUG
    std::cout << "Reader Started." << std::endl;
    #endif

    std::thread writer_thread(
        &echo::EchoWriter::start, &echo_writer_
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
        lk.lock();
        signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
        lk.unlock();
        // This switch statement acts as a router.
        switch(signal_ptr_->load()){
            case echo::READ_THREAD:
                // Read from user.
                read_mbox_ptr_->mbx_mtx.lock();
                sctp::sctp_message rcvdmsg(read_mbox_ptr_->rcvdmsg);
                read_mbox_ptr_->mbx_mtx.unlock();

                // Set Signals
                signal_ptr_->fetch_and(~0x0001, std::memory_order::memory_order_relaxed);
                read_mbox_ptr_->msg_flag.store(false);
                read_mbox_ptr_->mbx_cv.notify_all();

                // Create an echo message.
                // Explicitly Copy Construct.
                sctp::sctp_message sndmsg(app_.schedule(rcvdmsg));

                // Send the echo message back to the user.
                write_mbox_ptr_->mbx_mtx.lock();
                write_mbox_ptr_->sndmsg = sndmsg;
                write_mbox_ptr_->mbx_mtx.unlock();
                // Set Signals
                write_mbox_ptr_->msg_flag.store(true);
                write_mbox_ptr_->mbx_cv.notify_all();
                break;
            // case echo::TcpReader
        }
        #ifdef DEBUG
        if (++debug_counter > 4){
            break;
        }
        #endif
    }

    #ifdef DEBUG
    std::cout << "Scheduling Loop Ended." << std::endl;
    #endif

    // SIGTERM all threads.
    // Initiate graceful shutdown of SCTP associations.
    std::vector<echo::ExecutionContext > table_copy(context_table_);
    std::vector<echo::ExecutionContext > cancel_table;
    for (echo::ExecutionContext context: table_copy){
        app_.deschedule(context);
        cancel_table.push_back(context);
    }

    #ifdef DEBUG
    std::cout << "Closing the reader thread." << std::endl;
    #endif

    // Give the reader an opportunity to clean itself up.
    read_mbox_ptr_->signal.store(echo::Signals::TERMINATE);
    read_mbox_ptr_->mbx_cv.notify_all();
    sched_yield();
    
    // Force terminate the read side.
    pthread_cancel(read_thread.native_handle());

    // Wait a `reasonable' amount of time for worker threads to finish their current tasks.
    sleep(1);

    #ifdef DEBUG
    std::cout << "Terminating the worker threads." << std::endl;
    #endif

    // Force terminate the worker threads.
    for ( echo::ExecutionContext context: cancel_table ){
        #ifdef DEBUG
        std::cout << "Force terminate context: " << context.get_tid() << std::endl;
        #endif
        pthread_cancel(context.get_tid());
    }
    #ifdef DEBUG
    std::cout << "Terminating the writer thread." << std::endl;
    #endif

    // Give the write side a chance to clean itself up.
    s_ptr_->stop();
    write_mbox_ptr_->signal.store(echo::Signals::TERMINATE);
    write_mbox_ptr_->mbx_cv.notify_all();
    sched_yield();

    // Force Terminate the Writing thread.
    pthread_cancel(writer_thread.native_handle());

    read_thread.join();
    writer_thread.join();            
}