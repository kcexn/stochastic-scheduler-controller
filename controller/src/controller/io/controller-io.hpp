#ifndef CONTROLLER_IO_HPP
#define CONTROLLER_IO_HPP
#include "../../echo-app/utils/common.hpp"
#include "transport-servers/unix-server/unix-server.hpp"
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

        ~IO();
    private:
        std::shared_ptr<echo::MailBox> mbox_ptr_;
        pthread_t io_;
        // boost::asio::io_context ioc1_;
        boost::asio::io_context& ioc_;
        
        // Unix Socket Server
        UnixServer::unix_server us_;
    };
}// namespace io
}// namespace controller
#endif