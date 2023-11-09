#include "session.hpp"
#include "server.hpp"
#include <iostream>

namespace server
{
    bool Session::is_in_server(){
        auto self = shared_from_this();
        return server_.has(self);
    }
    void Session::erase(){
        if(!weak_from_this().expired()){
            server_.rm(shared_from_this());
        }
        return;
    }
}