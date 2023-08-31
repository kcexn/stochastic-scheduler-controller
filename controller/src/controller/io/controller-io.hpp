#ifndef CONTROLLER_IO_HPP
#define CONTROLLER_IO_HPP
#include "../../echo-app/utils/common.hpp"
#include "../../unix-server/unix-server.hpp"
#include <thread>
namespace controller{
namespace io{
    // This class encapsulates all of the io operations we need for the 
    // controller
    class IO
    {
    public:
        // IO(std::shared_ptr<echo::MailBox>& mbox);
        // IO(std::shared_ptr<echo::MailBox>& mbox, const std::string& local_endpoint);
        IO(std::shared_ptr<echo::MailBox>& mbox, const std::string& local_endpoint, boost::asio::io_context& ioc);
        void start();
        void stop();

        // Unix Socket Related Functions.
        void async_unix_read(const std::shared_ptr<UnixServer::Session>& session);
        void async_unix_accept();
        void async_unix_write(const boost::asio::const_buffer& write_buffer, const std::shared_ptr<UnixServer::Session>& unix_session_ptr, std::function<void(UnixServer::Session&)> fn);
        ~IO();
    private:
        std::shared_ptr<echo::MailBox> mbox_ptr_;
        pthread_t io_;
        // boost::asio::io_context ioc1_;
        boost::asio::io_context& ioc_;
        
        // Unix Socket Controls.
        std::vector<std::shared_ptr<UnixServer::Session> > unix_session_ptrs_;
        boost::asio::local::stream_protocol::endpoint endpoint_;
        UnixServer::Server unix_socket_server_;
    };
}// namespace io
}// namespace controller
#endif