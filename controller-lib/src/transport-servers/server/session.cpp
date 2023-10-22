#include "session.hpp"
#include "server.hpp"
#include <iostream>

namespace server
{
    void Session::erase(){
        if(!weak_from_this().expired()){
            server_.rm(shared_from_this());
        }
        return;
    }
}