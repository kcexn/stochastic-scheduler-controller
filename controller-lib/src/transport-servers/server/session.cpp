#include "session.hpp"
#include "server.hpp"

namespace server
{
    void Session::erase(){
        server_.rm(shared_from_this());
        return;
    }
}