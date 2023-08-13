#include "../echo.hpp"
#include <sys/wait.h>
#include <mutex>

#ifdef DEBUG
#include <iostream>
#endif

echo::Worker::Worker(
    std::shared_ptr<MailBox> mbox_ptr
) : mbox_ptr_(mbox_ptr)
{
    #ifdef DEBUG
    std::cout << "Worker Constructor." << std::endl;
    #endif
}

void echo::Worker::start(){
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
}