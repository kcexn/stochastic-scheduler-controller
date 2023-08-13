#include "echo.hpp"
#include "components/echo_reader.hpp"
#include "components/echo_writer.hpp"

#include <atomic>
#include <cstdlib>
#include <csignal>
#include <mutex>
#include <thread>
#include <functional>

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

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
    echo::EchoReader reader(
        s_ptr_,
        signal_mtx_ptr_,
        read_mbox_ptr,
        signal_ptr_,
        signal_cv_ptr_
    );
    std::thread read_thread(
        &echo::EchoReader::start, &reader
    );

    #ifdef DEBUG
    std::cout << "Reader Started." << std::endl;
    #endif

    std::shared_ptr<echo::MailBox> write_mbox_ptr = std::make_shared<echo::MailBox>();
    echo::EchoWriter writer(
        s_ptr_,
        signal_mtx_ptr_,
        write_mbox_ptr,
        signal_ptr_,
        signal_cv_ptr_
    );
    std::thread writer_thread(
        &echo::EchoWriter::start, &writer
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
        if (++debug_counter > 5){
            break;
        }
        #endif

        lk.lock();
        signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
        lk.unlock();
        // This switch statement acts as a router.
        switch(signal_ptr_->load()){
            case echo::READ_THREAD:
                // Read from user.
                read_mbox_ptr->mbx_mtx.lock();
                sctp::sctp_message rcvdmsg = read_mbox_ptr->rcvdmsg;
                read_mbox_ptr->mbx_mtx.unlock();

                // Set Signals
                signal_ptr_->fetch_and(~0x0001, std::memory_order::memory_order_relaxed);
                read_mbox_ptr->msg_flag.store(false);
                read_mbox_ptr->mbx_cv.notify_all();

                // Create an echo message.
                // Copy construct.
                sctp::sctp_message sndmsg(echo_(rcvdmsg));

                // Send the echo message back to the user.
                write_mbox_ptr->mbx_mtx.lock();
                write_mbox_ptr->sndmsg = sndmsg;
                write_mbox_ptr->mbx_mtx.unlock();
                // Set Signals
                write_mbox_ptr->msg_flag.store(true);
                write_mbox_ptr->mbx_cv.notify_all();
                break;
        }
    }

    #ifdef DEBUG
    std::cout << "Scheduling Loop Ended." << std::endl;
    #endif

    // SIGTERM all threads.
    // Initiate graceful shutdown of SCTP associations.
    for ( std::shared_ptr<MailBox> result: results ){
        sctp::endpoint remote = result->rcvdmsg.rmt_endpt.endpt;
        sctp::assoc_t assoc_id = result->rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id;
        #ifdef DEBUG
        std::cout << "Shutting down association: " << assoc_id << std::endl;
        #endif
        s_ptr_->shutdown_read(remote, assoc_id);
    }

    #ifdef DEBUG
    std::cout << "Closing the reader thread." << std::endl;
    #endif

    // Give the reader an opportunity to clean itself up.
    read_mbox_ptr->signal.store(echo::TERMINATE);
    read_mbox_ptr->mbx_cv.notify_all();
    sched_yield();
    
    // Force terminate the read side.
    pthread_cancel(read_thread.native_handle());

    #ifdef DEBUG
    std::cout << "Terminating the worker threads." << std::endl;
    #endif

    // Wait a `reasonable' amount of time for worker threads to 
    // finish their current tasks
    sleep(1);

    // Give the worker threads a chance to clean themselves up.
    for (std::size_t i=0; i < results.size(); ++i){
        results[i]->signal.store(echo::TERMINATE);
        results[i]->mbx_cv.notify_all();
    }
    sched_yield();

    // Force terminate the worker threads.
    for ( sctp_server::sctp_stream stream: stream_table ){
        pthread_cancel(stream.get_tid());
    }

    #ifdef DEBUG
    std::cout << "Terminating the writer thread." << std::endl;
    #endif

    // Give the write side a chance to clean itself up.
    s_ptr_->stop();
    write_mbox_ptr->signal.store(echo::TERMINATE);
    write_mbox_ptr->mbx_cv.notify_all();
    sched_yield();

    // Force Terminate the Writing thread.
    pthread_cancel(writer_thread.native_handle());

    read_thread.join();
    writer_thread.join();
}

// Echo is an application that manages the state of echo threads.
sctp::sctp_message echo::app::echo_(sctp::sctp_message& rcvdmsg){

    #ifdef DEBUG
    std::cout << "Entered the Echo Scheduler." << std::endl;
    #endif

    // Begin processing the echo application requests.
    sctp_server::sctp_stream strm = sctp_server::sctp_stream(
        rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id,
        rcvdmsg.rmt_endpt.rcvinfo.rcv_sid
    );
    std::size_t strm_idx = 0;
    for(; strm_idx < stream_table.size(); ++strm_idx){
        if (stream_table[strm_idx] == strm){
            break;
        }
    }
    if (strm_idx == stream_table.size()){
        // stream isn't in the table.
        stream_table.push_back(std::move(strm));
        std::shared_ptr<echo::MailBox> mbox_ptr = std::make_shared<echo::MailBox>();
        results.push_back(std::move(mbox_ptr));

        #ifdef DEBUG
        std::cout << "Initialize the echo input." << std::endl;
        #endif

        results.back()->rcvdmsg = rcvdmsg;
        results.back()->msg_flag.store(true);

        std::thread t(
            [](std::shared_ptr<echo::MailBox> mbox_ptr_){

                #ifdef DEBUG
                std::cout << "Echo Thread Initialized." << std::endl;
                #endif

                // Initialize Message Control Information
                std::unique_lock<std::mutex> mbox_lk(mbox_ptr_->mbx_mtx, std::defer_lock);
                #ifdef DEBUG
                std::cout << "Message Box Controls Initialized." << std::endl;
                #endif

                while((mbox_ptr_->signal.load() & echo::TERMINATE) != echo::TERMINATE){
                    mbox_lk.lock();
                    mbox_ptr_->mbx_cv.wait(mbox_lk, [&]{ return (mbox_ptr_->msg_flag.load() == true || mbox_ptr_->signal.load() != 0); });
                    sctp::sctp_message rcvdmsg = mbox_ptr_->rcvdmsg;
                    mbox_lk.unlock();
                    if ( (mbox_ptr_->signal.load() & echo::TERMINATE) == echo::TERMINATE ){
                        #ifdef DEBUG
                        std::cout << "Echo Worker Closing." << std::endl;
                        #endif
                        pthread_exit(0);
                    }

                    //Declare two pipes fds
                    int downstream[2] = {};
                    int upstream[2] = {};

                    //syscall return two pipes.
                    if (pipe(downstream) == -1){
                        perror("Downstream pipe failed to open.");
                    }
                    if (pipe(upstream) == -1){
                        perror("Upstream pip failed to open.");
                    }

                    // Fork Exec goes here.
                    pid_t pid = fork();
                    if(pid == 0){
                        //Child Process.

                        #ifdef DEBUG
                        std::cout << "Child Process about to exec cat." << std::endl;
                        #endif

                        if( close(downstream[1]) == -1 ){
                            perror("closing the downstream write in the child process failed.");
                        }
                        if ( close(upstream[0]) == -1 ){
                            perror("closing the upstream read in the child process failed.");
                        }
                        if (dup2(downstream[0], STDIN_FILENO) == -1){
                            perror("Failed to map the downstream read to STDIN.");
                        }
                        if (dup2(upstream[1], STDOUT_FILENO) == -1){
                            perror("Failed to map the upstream write to STDOUT.");
                        }

                        std::vector<const char*> argv{"/usr/bin/cat", NULL};
                        execve("/usr/bin/cat", const_cast<char* const*>(argv.data()), NULL);
                        exit(1);
                    } else {
                        //Parent Process.
                        if( close(downstream[0]) == -1 ){
                            perror("closing the downstream read in the parent process failed.");
                        }
                        if ( close(upstream[1]) == -1 ){
                            perror("closing the upstream write in the parent process failed.");
                        }
                        if( write(downstream[1], rcvdmsg.payload.data(), rcvdmsg.payload.size()) == -1 ){
                            perror("Downstream write in the parent process failed.");
                        }

                        char* buffer = new char[rcvdmsg.payload.size()]();
                        int length = read(upstream[0], buffer, rcvdmsg.payload.size());
                        if ( length == -1 ){
                            perror("Upstream read in the parent process failed.");
                        }

                        #ifdef DEBUG
                        std::cout << "Parent Process Write to stdout." << std::endl;
                        std::cout.write(buffer, length) << std::flush;
                        #endif

                        // Construct the echo message.
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
                            .payload = std::move(boost::asio::const_buffer(buffer, length))
                        };
                        mbox_lk.lock();
                        mbox_ptr_->sndmsg = sndmsg;
                        mbox_lk.unlock();
                        mbox_ptr_->msg_flag.store(false);
                        mbox_ptr_->mbx_cv.notify_all();
                    }

                    int status;
                    pid_t child_returned = waitpid( pid, &status, WNOHANG);
                    switch (child_returned){
                        case 0:
                            // Child Process with specified PID exists, but has not exited normally yet.
                            // At this stage in the thread though, the output from the process has already been collected.
                            // The only reason we would be in this state is if the application is hanging for some reason.
                            // For example, `cat' glues STDIN to STDOUT and stays running until it receives a signal.
                            // The scheduler thread at this stage will send a SIGTERM to the child process, and performa a blocking wait.
                            if ( kill(pid, SIGTERM) == -1 ){
                                perror("child process failed to terminate.");
                            }
                            if ( waitpid( pid, &status, 0) == -1 ){
                                perror("Wait on child has failed.");
                            }
                            break;
                        case -1:
                            perror("Wait on child pid failed.");
                            break;
                    }
                }
            }, results.back()
        );

        // retrieve sendmessage from thread.
        std::unique_lock<std::mutex> lk(results.back()->mbx_mtx);
        results.back()->mbx_cv.wait(lk, [&](){ return results.back()->msg_flag.load() == false; });
        sctp::sctp_message sndmsg = results.back()->sndmsg;
        lk.unlock();

        stream_table.back().set_tid(t.native_handle());
        t.detach();
        return sndmsg;
    } else {
        // stream is already in the stream table.
        std::unique_lock<std::mutex> lk(results[strm_idx]->mbx_mtx);
        results[strm_idx]->rcvdmsg = rcvdmsg;
        lk.unlock();
        results[strm_idx]->msg_flag.store(true);
        results[strm_idx]->mbx_cv.notify_all();

        // Acquire the condition variable and store the output.
        lk.lock();
        results[strm_idx]->mbx_cv.wait(lk, [&](){ return results[strm_idx]->msg_flag.load() == false; });
        sctp::sctp_message sndmsg = results[strm_idx]->sndmsg;
        lk.unlock();

        return sndmsg;
    }
}