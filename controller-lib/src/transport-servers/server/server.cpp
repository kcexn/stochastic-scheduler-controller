#include "server.hpp"
#include "session.hpp"
#include <boost/asio.hpp>

namespace server{
    void Server::run(){
        ioc_.run();
        return;
    }

    void Server::rm(const std::shared_ptr<Session>& session){
        acquire();
        auto it = std::find(cbegin(), cend(), session);
        if(it != cend()){
            erase(it);
        }
        release();
        return;
    }

    bool Server::has(const std::shared_ptr<Session>& session){
        bool is_present = false;
        acquire();
        auto it = std::find(cbegin(), cend(), session);
        if(it != cend()){
            is_present = true;
        }
        release();
        return is_present;
    }
}