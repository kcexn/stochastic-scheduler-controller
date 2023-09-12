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
                                session->cancel();
                                session->close();
                                session->erase();
                                passed_ = true;
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


}// namespace tests.