#include "echo-scheduler.hpp"

namespace echo{
    Scheduler::Scheduler(
        boost::asio::io_context& ioc, 
        short port,
        std::shared_ptr<std::mutex> signal_mtx_ptr,
        std::shared_ptr<std::atomic<int> > signal_ptr,
        std::shared_ptr<std::condition_variable> signal_cv_ptr
    ) : s_ptr_(std::make_shared<sctp_server::server>(ioc, port)),
        us_ptr_(std::make_shared<UnixServer::Server>(ioc)),
        signal_mtx_ptr_(signal_mtx_ptr),
        signal_ptr_(signal_ptr),
        signal_cv_ptr_(signal_cv_ptr),
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
        controller_mbox_ptr_(std::make_shared<MailBox>()),
        controller_ptr_(std::make_shared<controller::app::Controller>(
            controller_mbox_ptr_
        )),
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
        controller_mbox_ptr_->sched_signal_mtx_ptr = signal_mtx_ptr_;
        controller_mbox_ptr_->sched_signal_ptr = signal_ptr_;
        controller_mbox_ptr_->sched_signal_cv_ptr = signal_cv_ptr_;


    }
    #ifdef DEBUG
    Scheduler::~Scheduler(){
        std::cout << "Scheduler Destructor!" << std::endl;
    }
    #endif

    void Scheduler::start(){
        #ifdef DEBUG
        std::cout << "Scheduler Called!" << std::endl;
        #endif

        std::thread read_thread(
            &EchoReader::start, std::ref(echo_reader_)
        );
        #ifdef DEBUG
        std::cout << "Reader Started." << std::endl;
        #endif

        std::thread writer_thread(
            &EchoWriter::start, std::ref(echo_writer_)
        );
        #ifdef DEBUG
        std::cout << "Writer Thread Started." << std::endl;
        #endif

        // Start the scheduling loop.
        #ifdef DEBUG
        std::cout << "Scheduling Loop Started." << std::endl;
        #endif
        while( (signal_ptr_->load() & Signals::TERMINATE) != Signals::TERMINATE ){
            std::unique_lock<std::mutex> lk(*signal_mtx_ptr_);
            signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
            lk.unlock();
            // These if statements act as a router.
            std::atomic<int> signal = signal_ptr_->load();
            if( (signal & Signals::READ_THREAD) == Signals::READ_THREAD){
                // Read from the reader thread.
                read_mbox_ptr_->mbx_mtx.lock();
                sctp::sctp_message rcvdmsg(read_mbox_ptr_->rcvdmsg);
                read_mbox_ptr_->mbx_mtx.unlock();

                // Unset the READ THREAD signals.
                signal_ptr_->fetch_and(~Signals::READ_THREAD, std::memory_order::memory_order_relaxed);
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
                write_mbox_ptr_->signal.fetch_or(Signals::SCTP_WRITE, std::memory_order::memory_order_relaxed);
                write_mbox_ptr_->mbx_cv.notify_all();
            } else if ( (signal & Signals::UNIX_READ) == Signals::UNIX_READ){
                // Read from the reader thread.
                read_mbox_ptr_->mbx_mtx.lock();
                std::shared_ptr<UnixServer::Session> session_ptr(read_mbox_ptr_->session_ptr);
                read_mbox_ptr_->mbx_mtx.unlock();
                //Unset the UNIX_READ signals.
                signal_ptr_->fetch_and(~Signals::UNIX_READ, std::memory_order::memory_order_relaxed);
                read_mbox_ptr_->msg_flag.store(false);
                read_mbox_ptr_->mbx_cv.notify_all();

                // Pass the message to the application controller.
                controller_mbox_ptr_->mbx_mtx.lock();
                controller_mbox_ptr_->session_ptr = session_ptr;
                controller_mbox_ptr_->mbx_mtx.unlock();
                controller_mbox_ptr_->msg_flag.store(true);
                controller_mbox_ptr_->mbx_cv.notify_all();

            } else if ( (signal & Signals::SCHED_START) == Signals::SCHED_START ){
                signal_ptr_->fetch_and(~Signals::SCHED_START, std::memory_order::memory_order_relaxed);
                controller_mbox_ptr_->mbx_mtx.lock();
                std::vector<char> payload(controller_mbox_ptr_->payload_buffer_ptr->begin(), controller_mbox_ptr_->payload_buffer_ptr->end());
                controller_mbox_ptr_->mbx_mtx.unlock();

                #ifdef DEBUG
                std::cout.write(payload.data(), payload.size()) << std::endl;
                #endif

                // Echo back to Controller.
                controller_mbox_ptr_->mbx_mtx.lock();
                
            } else if ( (signal & Signals::APP_UNIX_WRITE ) == Signals::APP_UNIX_WRITE ){
                signal_ptr_->fetch_and(~Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                controller_mbox_ptr_->mbx_mtx.lock();
                std::vector<char> payload(controller_mbox_ptr_->payload_buffer_ptr->begin(), controller_mbox_ptr_->payload_buffer_ptr->end());
                std::shared_ptr<UnixServer::Session> session_ptr(controller_mbox_ptr_->session_ptr);
                controller_mbox_ptr_->mbx_mtx.unlock();
                controller_mbox_ptr_->signal.fetch_and(~Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                controller_mbox_ptr_->mbx_cv.notify_all();

                #ifdef DEBUG
                std::cout << "Pass Payload to Writer." << std::endl;
                std::cout.write(payload.data(), payload.size()) << std::endl;
                #endif

                // Send the message to the user.
                std::unique_lock<std::mutex> write_mbox_lock(write_mbox_ptr_->mbx_mtx);
                write_mbox_ptr_->mbx_cv.wait(write_mbox_lock, [&](){ return (write_mbox_ptr_->msg_flag.load() == false); });
                write_mbox_ptr_->payload_buffer_ptr = std::make_shared<std::vector<char> >(payload.begin(), payload.end());
                write_mbox_ptr_->session_ptr = session_ptr;
                write_mbox_lock.unlock();
                // Set Write Thread Signals.
                write_mbox_ptr_->signal.fetch_or(Signals::APP_UNIX_WRITE, std::memory_order::memory_order_relaxed);
                write_mbox_ptr_->msg_flag.store(true);
                write_mbox_ptr_->mbx_cv.notify_all();
            } else if ( (signal & Signals::TERMINATE) == Signals::TERMINATE ){
                break;
            }
        }

        #ifdef DEBUG
        std::cout << "Scheduling Loop Ended." << std::endl;
        #endif

        // SIGTERM all threads.
        // Initiate graceful shutdown of SCTP associations.
        std::vector<ExecutionContext > table_copy(context_table_);
        std::vector<ExecutionContext > cancel_table;
        for (ExecutionContext context: table_copy){
            sched_yield();
            app_.deschedule(context);
            cancel_table.push_back(context);
            sched_yield();
        }

        #ifdef DEBUG
        std::cout << "Closing the reader thread." << std::endl;
        #endif

        // Give the reader an opportunity to clean itself up.
        read_mbox_ptr_->signal.store(Signals::TERMINATE);
        read_mbox_ptr_->mbx_cv.notify_all();
        sched_yield();
        ioc_.stop();

        #ifdef DEBUG
        std::cout << "Terminating the worker threads." << std::endl;
        #endif
        // Sleep for 50ms
        struct timespec ts = {0, 50000000};
        nanosleep(&ts, NULL);

        // Force terminate the worker threads.
        for ( ExecutionContext context: cancel_table ){
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

        // Give the writer thread an opportunity to clean itself up.
        write_mbox_ptr_->signal.store(Signals::TERMINATE);
        write_mbox_ptr_->mbx_cv.notify_all();
        nanosleep(&ts, NULL);

        // Force Terminate the Writing thread.
        pthread_cancel(writer_thread.native_handle());

        read_thread.join();
        writer_thread.join();
    }

    void Scheduler::run(){
        ioc_.run();
    }
}//namespace echo.