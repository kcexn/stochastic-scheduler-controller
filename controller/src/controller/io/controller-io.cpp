#include "controller-io.hpp"
#include <filesystem>

#ifdef DEBUG
#include <iostream>
#endif
namespace controller{
namespace io{
    // IO::IO(std::shared_ptr<echo::MailBox>& mbox)
    //   : mbox_ptr_(mbox),
    //     ioc1_(),
    //     unix_socket_server_(ioc_, boost::asio::local::stream_protocol::endpoint("/run/controller/controller2.sock"))
    // { 
    //     #ifdef DEBUG
    //     std::cout << "IO Constructor!" << std::endl;
    //     #endif
    //     std::thread io(
    //         &IO::start, this
    //     );
    //     io_ = io.native_handle();
    //     io.detach();
    // }

    // IO::IO(std::shared_ptr<echo::MailBox>& mbox, const std::string& local_endpoint)
    //   : mbox_ptr_(mbox),
    //     ioc1_(),
    //     endpoint_(local_endpoint),
    //     unix_socket_server_(ioc1_, boost::asio::local::stream_protocol::endpoint(local_endpoint))
    // { 
    //     #ifdef DEBUG
    //     std::cout << "Endpoint Parameterized IO Constructor!" << std::endl;
    //     #endif
    //     std::thread io(
    //         &IO::start, this
    //     );
    //     io_ = io.native_handle();
    //     io.detach();
    // }

    IO::IO(std::shared_ptr<echo::MailBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc)
      : mbox_ptr_(mbox),
        ioc_(ioc),
        endpoint_(local_endpoint),
        unix_socket_server_(ioc, boost::asio::local::stream_protocol::endpoint(local_endpoint))
    { 
        #ifdef DEBUG
        std::cout << "Endpoint Parameterized IO Constructor!" << std::endl;
        #endif
        std::thread io(
            &IO::start, this
        );
        io_ = io.native_handle();
        io.detach();
    }


    void IO::start(){
        #ifdef DEBUG
        std::cout << "IO Thread started!" << std::endl;
        #endif
        async_unix_accept();
        ioc_.run();
        #ifdef DEBUG
        std::cout << "IO thread has stopped." << std::endl;
        #endif
        pthread_exit(0);
    }

    void IO::async_unix_read(const std::shared_ptr<UnixServer::Session>& session){
        session->async_read(
            [&, session](boost::system::error_code ec, std::size_t length) {
                if(!ec){
                    session->stream().write(session->sockbuf().data(), length);
                    std::unique_lock<std::mutex> lk(mbox_ptr_->mbx_mtx);
                    mbox_ptr_->mbx_cv.wait(lk, [&]{ return (mbox_ptr_->msg_flag.load(std::memory_order::memory_order_relaxed) == false || mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed) != 0); });
                    int thread_local_signal = mbox_ptr_->signal.load(std::memory_order::memory_order_relaxed);
                    mbox_ptr_->session_ptr = std::shared_ptr<UnixServer::Session>(session);
                    mbox_ptr_->msg_flag.store(true, std::memory_order::memory_order_relaxed);
                    mbox_ptr_->sched_signal_ptr->fetch_or(echo::Signals::UNIX_READ, std::memory_order::memory_order_relaxed);
                    lk.unlock();
                    mbox_ptr_->sched_signal_cv_ptr->notify_all();
                    if( (thread_local_signal & echo::Signals::TERMINATE) == echo::Signals::TERMINATE ){
                        pthread_exit(0);
                    }
                    async_unix_read(session);
                }
            }
        );
    }

    void IO::async_unix_accept()
    {
        unix_socket_server_.start_accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                if (!ec){
                    #ifdef DEBUG
                    std::cout << "Accept a Unix Socket Connection." << std::endl;
                    #endif
                    unix_session_ptrs_.emplace_back(std::move(std::make_shared<UnixServer::Session>(std::move(socket))));
                    async_unix_read(unix_session_ptrs_.back());
                    async_unix_accept();
                }
            }
        );
    }

    void IO::async_unix_write(const boost::asio::const_buffer& write_buffer, const std::shared_ptr<UnixServer::Session>& unix_session_ptr, std::function<void(UnixServer::Session&)> fn){
        unix_session_ptr->async_write(write_buffer, fn);
    }

    void IO::stop(){
        ioc_.stop();
        for (auto session_ptr: unix_session_ptrs_){
            session_ptr->cancel_reads();
        }
        unix_session_ptrs_.clear();

        // Wait a reasonable amount of time for the IO thread to stop.
        struct timespec ts = {0,50000000};
        nanosleep(&ts, nullptr);

        ioc_.restart();
        while( ioc_.poll() > 0 ){}
        std::filesystem::path socket_address;
        if ( endpoint_ == boost::asio::local::stream_protocol::endpoint()){
            socket_address = std::filesystem::path("/run/controller/controller.sock");
        } else {
            socket_address = std::filesystem::path(endpoint_.path());
        }
        std::filesystem::remove(socket_address);
    }

    IO::~IO()
    {
        #ifdef DEBUG
        std::cout << "IO Destructor!" << std::endl;
        #endif
        stop();
    }
}//namespace io
}//namespace controller