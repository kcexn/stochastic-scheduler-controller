#include "echo-scheduler.hpp"

echo::Scheduler::Scheduler(boost::asio::io_context& ioc, short port)
      : s_ptr_(std::make_shared<sctp_server::server>(ioc, port)),
        us_ptr_(std::make_shared<UnixServer::Server>(ioc)),
        signal_mtx_ptr_(std::make_shared<std::mutex>()),
        signal_ptr_(std::make_shared<std::atomic<int >>()),
        signal_cv_ptr_(std::make_shared<std::condition_variable>()),
        read_mbox_ptr_(std::make_shared<MailBox>()),
        write_mbox_ptr_(std::make_shared<MailBox>()),
        echo_reader_(
            s_ptr_,
            us_ptr_,
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
        ioc_(ioc) 
{
    #ifdef DEBUG
    std::cout << "Scheduler Constructor!" << std::endl;
    #endif
}
#ifdef DEBUG
echo::Scheduler::~Scheduler(){
    std::cout << "Scheduler Destructor!" << std::endl;
}
#endif

void echo::Scheduler::start(){
    #ifdef DEBUG
    int debug_counter = 0;
    std::cout << "Scheduler Called!" << std::endl;
    #endif

    std::thread read_thread(
        &echo::EchoReader::start, std::ref(echo_reader_)
    );
    #ifdef DEBUG
    std::cout << "Reader Started." << std::endl;
    #endif

    std::thread writer_thread(
        &echo::EchoWriter::start, std::ref(echo_writer_)
    );
    #ifdef DEBUG
    std::cout << "Writer Thread Started." << std::endl;
    #endif

    // Start the scheduling loop.
    #ifdef DEBUG
    std::cout << "Scheduling Loop Started." << std::endl;
    #endif
    while(true){
        std::unique_lock<std::mutex> lk(*signal_mtx_ptr_);
        signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
        lk.unlock();
        // These if statements act as a router.
        std::atomic<int> signal = signal_ptr_->load();
        if( (signal & echo::Signals::READ_THREAD) == echo::Signals::READ_THREAD){
            // Read from the reader thread.
            read_mbox_ptr_->mbx_mtx.lock();
            sctp::sctp_message rcvdmsg(read_mbox_ptr_->rcvdmsg);
            read_mbox_ptr_->mbx_mtx.unlock();

            // Unset the READ THREAD signals.
            signal_ptr_->fetch_and(~echo::Signals::READ_THREAD, std::memory_order::memory_order_relaxed);
            read_mbox_ptr_->msg_flag.store(false);
            read_mbox_ptr_->mbx_cv.notify_all();

            // Create an echo message.
            // Explicitly Copy Construct.
            sctp::sctp_message sndmsg(app_.schedule(rcvdmsg));

            // Send the echo message back to the user.
            write_mbox_ptr_->mbx_mtx.lock();
            write_mbox_ptr_->sndmsg = sndmsg;
            write_mbox_ptr_->mbx_mtx.unlock();
            // Set Write Thread Signals.
            write_mbox_ptr_->msg_flag.store(true);
            write_mbox_ptr_->signal.fetch_or(echo::Signals::SCTP_WRITE, std::memory_order::memory_order_relaxed);
            write_mbox_ptr_->mbx_cv.notify_all();
        } else if ( (signal & echo::Signals::UNIX_READ) == echo::Signals::UNIX_READ){
            // Read from the reader thread.
            read_mbox_ptr_->mbx_mtx.lock();
            std::shared_ptr<UnixServer::Session> session_ptr(read_mbox_ptr_->session_ptr);
            read_mbox_ptr_->mbx_mtx.unlock();
            //Unset the UNIX_READ signals.
            signal_ptr_->fetch_and(~echo::Signals::UNIX_READ, std::memory_order::memory_order_relaxed);
            read_mbox_ptr_->msg_flag.store(false);
            read_mbox_ptr_->mbx_cv.notify_all();

            // Send the message back to the user.
            write_mbox_ptr_->mbx_mtx.lock();
            write_mbox_ptr_->session_ptr = session_ptr;
            write_mbox_ptr_->mbx_mtx.unlock();
            // Set Write Thread Signals.
            write_mbox_ptr_->msg_flag.store(true);
            write_mbox_ptr_->signal.fetch_or(echo::Signals::UNIX_WRITE, std::memory_order::memory_order_relaxed);
            write_mbox_ptr_->mbx_cv.notify_all();
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
        sched_yield();
        app_.deschedule(context);
        cancel_table.push_back(context);
        sched_yield();
    }

    // Gracefully shutdown the read side of the Unix Socket Connections.
    std::vector<std::shared_ptr<UnixServer::Session> > session_ptrs = echo_reader_.sessions();
    for (std::shared_ptr<UnixServer::Session> session_ptr: session_ptrs){
        session_ptr->shutdown_read();
    }

    #ifdef DEBUG
    std::cout << "Closing the reader thread." << std::endl;
    #endif

    // Give the reader an opportunity to clean itself up.
    read_mbox_ptr_->signal.store(echo::Signals::TERMINATE);
    read_mbox_ptr_->mbx_cv.notify_all();
    sched_yield();
    ioc_.stop();

    #ifdef DEBUG
    std::cout << "Terminating the worker threads." << std::endl;
    #endif
    sleep(1);

    // Force terminate the worker threads.
    for ( echo::ExecutionContext context: cancel_table ){
        #ifdef DEBUG
        std::cout << "Force terminate context: " << context.get_tid() << std::endl;
        #endif
        pthread_cancel(context.get_tid());
        sched_yield();
    }
    #ifdef DEBUG
    std::cout << "Terminating the writer thread." << std::endl;
    #endif

    // Gracefully stop and close the SCTP associations,
    // Gracefully stop and close the iocontext.
    s_ptr_->stop();
    // Gracefully stop and close the Unix Sessions.
    for (std::shared_ptr<UnixServer::Session> session_ptr: session_ptrs){
        session_ptr->close();
    }

    // Give the writer thread an opportunity to clean itself up.
    write_mbox_ptr_->signal.store(echo::Signals::TERMINATE);
    write_mbox_ptr_->mbx_cv.notify_all();
    sched_yield();

    // Force Terminate the Writing thread.
    pthread_cancel(writer_thread.native_handle());
    sched_yield();

    read_thread.join();
    writer_thread.join();
}

void echo::Scheduler::run(){
    ioc_.run();
}