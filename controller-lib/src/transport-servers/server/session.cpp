#include "session.hpp"
#include "server.hpp"

namespace server
{
    void Session::erase(){
        server_.acquire();
        auto it = std::find(server_.begin(), server_.end(), shared_from_this());
        if(it != server_.end()){
            server_.erase(it);
        }
        server_.release();
        return;
    }
}