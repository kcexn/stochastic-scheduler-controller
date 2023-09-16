#include "sctp-server-tests.hpp"
#include "../../../src/transport-servers/sctp-server/sctp.hpp"
#include "../../../src/transport-servers/sctp-server/sctp-session.hpp"
#include <iostream>
namespace tests{
    SctpServerTests::SctpServerTests(DefaultConstructor, boost::asio::io_context& ioc)
    {
        sctp_transport::SctpServer sctp_server(ioc);
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(TestSocketRead, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc,endpoint);
        sctp_server.init([&](const boost::system::error_code& ec, std::shared_ptr<sctp_transport::SctpSession> session){
            session->async_read([session](const boost::system::error_code& ec, const std::size_t& len){
                if(!ec){
                    std::cout << session->acquire_stream().str() << std::flush;
                    session->release_stream();
                }
            });
        });
        ioc.run_for(std::chrono::duration<int>(5));
        sctp_server.stop();
        struct timespec ts = {1, 0};
        nanosleep(&ts, 0);
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(TestSessionConstructor, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc,endpoint);
        transport::protocols::sctp::stream_t stream_id = {
            1,
            2
        };
        transport::protocols::sctp::socket sock(ioc);
        sctp_transport::SctpSession sctp_session(sctp_server, stream_id, sock);
        sctp_transport::SctpSession sctp_session2(sctp_server, stream_id, sock);
        if(sctp_session != sctp_session2){
            return;
        } else if (sctp_session != stream_id){
            return;
        } else if (sctp_session2 != stream_id){
            return;
        }
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(TestSessionReadWrite, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc,endpoint);
        sctp_server.init([&](const boost::system::error_code& ec, std::shared_ptr<sctp_transport::SctpSession> session){
            session->async_read([session](const boost::system::error_code& ec, const std::size_t& len){
                if(!ec){
                    std::string echo(session->acquire_stream().str());
                    session->release_stream();
                    boost::asio::const_buffer buf(echo.data(), echo.size());
                    session->async_write(buf, [&, session](){
                        session->close();
                    });
                }
            });
        });
        ioc.run_for(std::chrono::duration<int>(5));
        passed_ = true;
    }

    SctpServerTests::SctpServerTests(TestSessionConnect, boost::asio::io_context& ioc, const transport::protocols::sctp::endpoint& endpoint)
    {
        sctp_transport::SctpServer sctp_server(ioc, endpoint);
        server::Remote rmt;
        rmt.tuple = {
            server::AddressType::TUPLE,
            {},
            IPPROTO_SCTP,
            {
                AF_INET,
                htons(6100),
                0
            }
        };
        inet_aton("127.0.0.1", &rmt.tuple.remote_addr.sin_addr);
        sctp_server.init([&](const boost::system::error_code& ec, std::shared_ptr<sctp_transport::SctpSession> session){ return; });
        std::shared_ptr<server::Session> session = sctp_server.async_connect(
            rmt,
            [&](const boost::system::error_code& ec){
                if(!ec){
                    std::string data("Hello World!\n");
                    boost::asio::const_buffer buf(data.data(), data.size());
                    session->async_write(
                        buf,
                        [&]{
                            passed_ = true;
                        }
                    );
                }
            }
        );
        ioc.run_for(std::chrono::duration<int>(5));
    }
}