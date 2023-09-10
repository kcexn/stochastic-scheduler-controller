#include "server-tests.hpp"
#include <chrono>
namespace tests{
    UnixServerTest::UnixServerTest(boost::asio::io_context& ioc)
      : passed_{false},
        server_(ioc)
    {
        passed_ = true;
    }

    UnixServerTest::UnixServerTest(boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : passed_{false},
        server_(ioc, endpoint)
    {
        passed_ = true;
    }

    UnixServerTest::UnixServerTest(UnixServerTest::TestPushBack, boost::asio::io_context& ioc)
      : passed_{false},
        server_(ioc)
    {
        server_.push_back(std::make_shared<UnixServer::unix_session>(ioc, server_));
        if(!server_.empty()){
            passed_ = true;
        }
    }

    UnixServerTest::UnixServerTest(UnixServerTest::TestContextRun, boost::asio::io_context& ioc)
      : passed_{false},
        server_(ioc)
    {
        std::chrono::duration<int> d(5);
        boost::asio::steady_timer t(ioc, d);
        t.async_wait(
            [&](const boost::system::error_code& ec){
                passed_ = true;
            }
        );
        server_.run();
    }

    UnixServerTest::UnixServerTest(UnixServerTest::TestAcceptUnix, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : passed_{false},
        server_(ioc, endpoint)
    {
        server_.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                server_.push_back(std::make_shared<UnixServer::unix_session>(std::move(socket), server_));
                if(!(server_.empty())){
                    passed_ = true;
                }
            }
        );
        server_.run();
    }

    UnixServerTest::UnixServerTest(UnixServerTest::TestReadWrite, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : passed_{false},
        server_(ioc, endpoint)
    {
        server_.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), server_);
                session->async_read(
                    [&, session](const boost::system::error_code& ec, std::size_t bytes_transferred){
                        session->stream().write(session->buf().data(), bytes_transferred);
                        boost::asio::const_buffer buf(session->buf().data(), bytes_transferred);
                        session->async_write(
                            buf,
                            [&, session](){
                                session->close();
                                passed_ = true;
                            }
                        );

                    }
                );
                server_.push_back(std::move(session));
            }
        );
        server_.run();
    }

    UnixServerTest::UnixServerTest(UnixServerTest::TestEraseSession, boost::asio::io_context& ioc, const boost::asio::local::stream_protocol::endpoint& endpoint)
      : passed_{false},
        server_(ioc, endpoint)
    {
        server_.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), server_);
                session->async_read(
                    [&, session](const boost::system::error_code& ec, std::size_t bytes_transferred){
                        session->stream().write(session->buf().data(), bytes_transferred);
                        boost::asio::const_buffer buf(session->buf().data(), bytes_transferred);
                        session->async_write(
                            buf,
                            [&, session](){
                                session->close();
                                session->erase();
                                if(server_.empty()){
                                    passed_ = true;
                                }
                            }
                        );
                    }
                );
            }
        );
        server_.run();
    }

    UnixSessionTest::UnixSessionTest(boost::asio::io_context& ioc, server::Server& server)
      : passed_{false},
        session_(ioc, server)
    {
        passed_ = true;
    }

    UnixSessionTest::UnixSessionTest(UnixSessionTest::TestMoveConstruct, boost::asio::local::stream_protocol::socket&& socket, server::Server& server)
      : passed_{false},
        session_(std::move(socket), server)
    {
        passed_ = true;
    }

}