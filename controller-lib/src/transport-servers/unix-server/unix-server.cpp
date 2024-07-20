#include "unix-server.hpp"
#include <filesystem>

#ifdef DEBUG
#include <sys/wait.h>
#endif

namespace UnixServer{
    void unix_session::async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn){
        socket_.async_read_some(
            boost::asio::buffer(buf().data(), SERVER_SESSION_MAX_BUFLEN),
            boost::asio::bind_cancellation_slot(
                stop_signal_.slot(),
                [&,fn](boost::system::error_code ec, std::size_t length){
                    fn(ec, length);
                    if(!ec){
                        async_read(fn);
                    } else {
                        if(ec == boost::asio::error::misc_errors::eof){
                            return;
                        } else {
                            std::cerr << "unix-server.cpp:18:async_read_some error:" << ec.message() << std::endl;
                            async_read(fn);
                        }
                    }
                    return;
                }
            )
        );
    }

    void unix_session::async_write(const boost::asio::const_buffer& write_buffer, const std::function<void(const std::error_code& ec)>& fn){
        #ifdef DEBUG
        struct timespec ts = {0,0};
        #endif
        std::shared_ptr<std::vector<char> > write_data_ptr = std::make_shared<std::vector<char> >(write_buffer.size());
        std::memcpy(write_data_ptr->data(), write_buffer.data(), write_data_ptr->size());
        boost::asio::const_buffer buf(write_data_ptr->data(), write_data_ptr->size());
        auto self = shared_from_this();
        socket_.async_write_some(
            buf,
            [&, buf, write_data_ptr, fn, self](const boost::system::error_code& ec, std::size_t bytes_transferred){
                if (!ec){
                    #ifdef DEBUG
                    clock_gettime(CLOCK_REALTIME, &ts); std::cerr << "unix-server.cpp:39:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":WRITE_UNIX_SOCKET_DATA:" << std::string((const char*)buf.data(), buf.size()) << std::endl;
                    #endif
                    std::size_t remaining_bytes = write_data_ptr->size() - bytes_transferred;
                    if ( remaining_bytes > 0 ){
                        async_write(buf + bytes_transferred, fn);
                    } else {
                        // Once the write is complete execute the completion
                        // handler.
                        std::error_code err;
                        fn(err);
                    }
                } else if (ec == boost::asio::error::would_block){
                    async_write(buf,fn);
                } else if (ec == boost::asio::error::broken_pipe){
                    /* I'm not sure that this is recoverable since it is likely caused by an NGINX gateway timeout.*/
                    throw boost::asio::error::broken_pipe;
                } else {
                    std::error_code err(ec.value(), std::system_category());
                    struct timespec ts = {};
                    int status = clock_gettime(CLOCK_REALTIME, &ts);
                    if(status == -1){
                        std::cerr << "unix-server.cpp:41:clock_gettime error:" << std::make_error_code(std::errc(errno)).message() << std::endl;
                        std::cerr << "unix-server.cpp:42:unix socket write error:" << ec.message() << \
                            ":value=" << std::string(write_data_ptr->begin(), write_data_ptr->end()) << \
                            ",len=" << write_data_ptr->size() << std::endl;
                    } else {
                        std::cerr << "unix-server.cpp:44:" << (ts.tv_sec*1000 + ts.tv_nsec/1000000) << ":unix socket write error:" << ec.message() \
                            << ":value=" << std::string(write_data_ptr->begin(), write_data_ptr->end()) \
                            << ",len=" << write_data_ptr->size() << std::endl;
                    }
                }
            }
        );
    }

    void unix_session::close(){
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

    void unix_server::accept(std::function<void(const boost::system::error_code& ec, std::shared_ptr<UnixServer::unix_session> session)> fn){
        acceptor_.async_accept([&, fn](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
            if(!ec){
                std::shared_ptr<UnixServer::unix_session> session =  std::make_shared<UnixServer::unix_session>(std::move(socket), *this);
                push_back(session);
                fn(ec, session);
                accept(fn);
            } else {
                std::cerr << ec.message() << std::endl;
            }
        });
    }

    unix_server::~unix_server(){
        clear();
        std::filesystem::path p(endpoint_.path());
        std::filesystem::remove(p);
    }
}// Unix Server Namespace