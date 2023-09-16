#ifndef CONTROLLER_IO_HPP
#define CONTROLLER_IO_HPP
#include <memory>
#include <transport-servers/sctp-server/sctp-server.hpp>
#include <transport-servers/unix-server/unix-server.hpp>
/*Forward Declarations*/
namespace boost{
namespace asio{
    class io_context;
}
}

namespace controller{
namespace io{
    struct MessageBox{
        // Child Thread Signals
        std::mutex mbx_mtx;
        std::condition_variable mbx_cv;
        std::atomic<std::uint16_t> signal;

        // Shared Flag.
        std::atomic<bool> msg_flag;

        // Parent Thread Signals
        std::shared_ptr<std::mutex> sched_signal_mtx_ptr;
        std::shared_ptr<std::atomic<std::uint16_t> > sched_signal_ptr;
        std::shared_ptr<std::condition_variable> sched_signal_cv_ptr;

        // Payload.
        std::shared_ptr<server::Session> session;
    };

    // This class encapsulates all of the io operations we need for the 
    // controller
    class IO
    {
    public:
        // IO(std::shared_ptr<echo::MailBox>& mbox);
        // IO(std::shared_ptr<echo::MailBox>& mbox, const std::string& local_endpoint);
        IO(std::shared_ptr<MessageBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc);
        void start();
        void stop();
        ~IO();
    private:
        std::shared_ptr<MessageBox> mbox_ptr_;
        pthread_t io_;
        // boost::asio::io_context ioc1_;
        boost::asio::io_context& ioc_;
        
        // Unix Socket Server
        UnixServer::unix_server us_;

        // SCTP Socket Server
        sctp_transport::SctpServer ss_;
    };
}// namespace io
}// namespace controller
#endif