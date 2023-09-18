#include "http-server-tests.hpp"
#include <filesystem>

namespace tests{
    HttpServerTests::HttpServerTests(HttpServerTests::ConstructServer)
      : passed_{false}
    {
        http::HttpServer server;
        std::size_t num_sessions = server.size();
        if(num_sessions != 0){
            return;
        }
        passed_ = true;
    }

    HttpServerTests::HttpServerTests(HttpServerTests::ConstructSession)
      : passed_{false}
    {
        http::HttpServer server;
        server.push_back(std::make_shared<http::HttpSession>(server));
        if(server.size() != 1){
            return;
        }
        passed_ = true;
    }

    HttpServerTests::HttpServerTests(HttpServerTests::ConstructUnixBoundSession)
      : passed_{false}
    {
        boost::asio::io_context ioc;
        std::filesystem::path addr("/run/controller/controller.sock");
        boost::asio::local::stream_protocol::endpoint endpoint(addr);
        UnixServer::unix_server us(ioc, endpoint);
        us.push_back(std::make_shared<UnixServer::unix_session>(ioc, us));
        http::HttpServer hs;
        hs.push_back(std::make_shared<http::HttpSession>(hs, us.back()));
        if(hs.size() != 1){
            return;
        }
        passed_ = true;
    }

    HttpServerTests::HttpServerTests(UnixBoundReadWrite)
      : passed_{false}
    {
        boost::asio::io_context ioc;
        std::filesystem::path addr("/run/controller/controller.sock");
        boost::asio::local::stream_protocol::endpoint endpoint(addr);
        UnixServer::unix_server us(ioc, endpoint);
        if(!std::filesystem::exists(addr)){
            return;
        }
        http::HttpServer hs;
        us.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), us);
                hs.push_back(std::make_shared<http::HttpSession>(hs, session));
                session->async_read(
                    [&, session](const boost::system::error_code& ec, std::size_t bytes_transferred){
                        session->acquire_stream().write(session->buf().data(), bytes_transferred);
                        session->release_stream();
                        hs.back()->read();
                        auto req_res = hs.back()->get();
                        std::cout << std::get<http::HttpRequest>(req_res) << std::flush;
                        std::get<http::HttpResponse>(req_res) = {
                            http::HttpVersion::V1_1,
                            http::HttpStatus::OK,
                            {
                                {http::HttpHeaderField::CONNECTION, "close"},
                                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                                {http::HttpHeaderField::END_OF_HEADERS, ""}
                            },
                            {
                                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
                            }
                        };
                        hs.back()->set(req_res);
                        hs.back()->write(
                            [&, session](){
                                hs.back()->close();
                            }
                        );
                    }
                );
                us.push_back(std::move(session));
            }
        );
        us.run();
        ioc.restart();
        us.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), us);
                hs.push_back(std::make_shared<http::HttpSession>(hs, session));
                session->async_read(
                    [&, session](const boost::system::error_code& ec, std::size_t bytes_transferred){
                        session->acquire_stream().write(session->buf().data(), bytes_transferred);
                        session->release_stream();
                        hs.back()->read();
                        http::HttpReqRes req_res;
                        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
                        res = {
                            http::HttpVersion::V1_1,
                            http::HttpStatus::OK,
                            {
                                {http::HttpHeaderField::CONNECTION, "close"},
                                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                                {http::HttpHeaderField::END_OF_HEADERS, ""}
                            },
                            {
                                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
                            }                            
                        };
                        hs.back()->write(
                            req_res,
                            [&, session](){
                                hs.back()->close();
                            }
                        );
                    }
                );
                us.push_back(std::move(session));
            }
        );
        us.run();
        ioc.restart();
        us.accept(
            [&](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket socket){
                std::shared_ptr<UnixServer::unix_session> session = std::make_shared<UnixServer::unix_session>(std::move(socket), us);
                hs.push_back(std::make_shared<http::HttpSession>(hs, session));
                session->async_read(
                    [&, session](const boost::system::error_code& ec, std::size_t bytes_transferred){
                        session->acquire_stream().write(session->buf().data(), bytes_transferred);
                        session->release_stream();
                        hs.back()->read();
                        http::HttpReqRes req_res = hs.back()->get();
                        http::HttpResponse& res = std::get<http::HttpResponse>(req_res);
                        res = {
                            http::HttpVersion::V1_1,
                            http::HttpStatus::OK,
                            {
                                {http::HttpHeaderField::CONNECTION, "close"},
                                {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                                {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                                {http::HttpHeaderField::END_OF_HEADERS, ""}
                            },
                            {
                                {http::HttpBigNum{22}, "{\"msg\":\"Hello World!\"}"}
                            }                            
                        };
                        (*hs.back()) = req_res;
                        hs.back()->write(
                            [&, session](){
                                hs.back()->close();
                            }
                        );
                    }
                );
                us.push_back(std::move(session));
            }
        );
        us.run();
        passed_ = true;
    }

    HttpServerTests::HttpServerTests(UnixBoundClientWrite)
      : passed_{false}
    {
        boost::asio::io_context ioc;
        std::filesystem::path addr("/run/controller/controller.sock");
        boost::asio::local::stream_protocol::endpoint endpoint(addr);
        UnixServer::unix_server us(ioc, endpoint);
        if(!std::filesystem::exists(addr)){
            return;
        }
        http::HttpServer hs;
        server::Remote rmt;
        rmt.unix_addr = {
            SOCK_STREAM,
            0,
            {
                AF_UNIX,
                "/run/controller/openwhisk.sock"
            }
        };
        us.async_connect(rmt, [&](const boost::system::error_code& ec, const std::shared_ptr<server::Session>& session){
            hs.push_back(
                std::make_shared<http::HttpClientSession>(hs, session)
            );
            std::shared_ptr<http::HttpClientSession> client_session = std::static_pointer_cast<http::HttpClientSession>(hs.back());
            http::HttpRequest& req = std::get<http::HttpRequest>(*client_session);
            req = {
                http::HttpVerb::POST,
                "/api/v1/namespaces/guest/actions",
                http::HttpVersion::V1_1,
                {
                    {http::HttpHeaderField::CONTENT_TYPE, "application/json"},
                    {http::HttpHeaderField::CONTENT_LENGTH, "22"},
                    {http::HttpHeaderField::ACCEPT, "application/json"},
                    {http::HttpHeaderField::HOST, "www.tests.com"},
                    {http::HttpHeaderField::END_OF_HEADERS, ""}
                },
                {
                    {{}, "{\"msg\":\"Hello World!\"}"}
                }
            };
            client_session->write([&, session](){
                struct timespec ts = {1,0};
                nanosleep(&ts, nullptr);

                // /* Example code: */
                // // After submitting the request write, we need to wait for a response read.
                // // First we register a callback for the transport session.
                // session->cb = [&, session](const auto& ec, auto len){
                //     if(!ec){
                //         session->acquire_stream().write(session->buf().data(), len);
                //         session->release_stream();
                //         session->async_read(session->cb);
                //     }
                //     return;
                // };
                // // Then we can submit the asynchronous reads.
                // session->async_read(session->cb);
                passed_ = true;
            });
        });
        us.run();
    }

}// namespace tests.