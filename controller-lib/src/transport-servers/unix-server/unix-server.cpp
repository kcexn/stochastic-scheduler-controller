#include "unix-server.hpp"
#include <filesystem>

namespace UnixServer{
    void unix_session::async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn){
        socket_.async_read_some(
            boost::asio::buffer(buf().data(), unix_session::max_buflen),
            boost::asio::bind_cancellation_slot(
                stop_signal_.slot(),
                [&,fn](boost::system::error_code ec, std::size_t length){
                    fn(ec, length);
                    if(ec != boost::asio::error::misc_errors::eof){
                        async_read(fn);
                    }
                }
            )
        );
    }

    void unix_session::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn){
            std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
            std::memcpy(write_data_ptr->data(), write_buffer.data(), write_data_ptr->size());
            boost::asio::const_buffer buf(write_data_ptr->data(), write_data_ptr->size());
            socket_.async_write_some(
                buf,
                [&, buf, write_data_ptr, fn](const boost::system::error_code& ec, std::size_t bytes_transferred){
                    if (!ec){
                        std::size_t remaining_bytes = write_data_ptr->size() - bytes_transferred;
                        if ( remaining_bytes > 0 ){
                            boost::asio::const_buffer buf = buf + bytes_transferred;
                            async_write(buf, fn);
                        } else {
                            // Once the write is complete execute the completion
                            // handler.
                            fn();
                        }
                    }
                }
            );
        }

    void unix_session::close(){
        cancel();
        erase();
    }

    void unix_session::async_connect(const boost::asio::local::stream_protocol::endpoint& endpoint, std::function<void(const boost::system::error_code&)> fn) {
        socket_.async_connect(endpoint, [&, fn](const boost::system::error_code& ec) {
            fn(ec);
        });
        return;
    }

    unix_server::unix_server(boost::asio::io_context& ioc)
      : server::Server(ioc),
        endpoint_("/run/controller/controller.sock"),
        acceptor_(ioc, endpoint_)
    {}

    unix_server::unix_server(boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : server::Server(ioc),
        endpoint_(endpoint),
        acceptor_(ioc, endpoint)
    {}

    void unix_server::async_connect(server::Remote rmt, std::function<void(const boost::system::error_code&, const std::shared_ptr<server::Session>&)> fn) {
        boost::asio::local::stream_protocol::socket sock(ioc_,boost::asio::local::stream_protocol());
        const char* path = rmt.unix_addr.address.sun_path;
        boost::asio::local::stream_protocol::endpoint endpoint(path);
        std::shared_ptr<unix_session> session = std::make_shared<unix_session>(std::move(sock), *this);
        session->async_connect(endpoint, [&, fn, session](const boost::system::error_code& ec) {
            if(!ec){
                push_back(session);
            }
            fn(ec, session);
        });
        return;
    }

    void unix_server::accept(std::function<void(const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket)> fn){
        acceptor_.async_accept(fn);
    }

    unix_server::~unix_server(){
        clear();
        std::filesystem::path p(endpoint_.path());
        std::filesystem::remove(p);
    }
}// Unix Server Namespace