#include "server.hpp"
#include <boost/asio.hpp>
namespace server{
    void Server::run(){
        ioc_.run();
        return;
    }
}