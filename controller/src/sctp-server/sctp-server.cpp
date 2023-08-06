#include "sctp-server.hpp"

sctp_server::sctp_server(boost::asio::io_context& ioc, short port)
    : socket_(ioc, sctp::endpoint(sctp::v4(), port))
{
    std::cout << "Local endpoint address: " << socket_.local_endpoint().address().to_string() << std::endl;
    std::cout << "Local endpoint port: " << std::to_string(socket_.local_endpoint().port()) << std::endl;
    std::cout << "Local endpoint protocol: " << std::to_string(socket_.local_endpoint().protocol().protocol()) << std::endl;
    std::cout << "Local endpoint type: " << std::to_string(socket_.local_endpoint().protocol().type()) << std::endl;
    std::cout << "Local endpoint family: " << std::to_string(socket_.local_endpoint().protocol().family()) << std::endl;
}