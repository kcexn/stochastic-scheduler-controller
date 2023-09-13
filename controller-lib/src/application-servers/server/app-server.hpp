#ifndef APP_SERVER_HPP
#define APP_SERVER_HPP
#include "app-session.hpp"

namespace app_server{
    // The application server is a container for managing
    // application sessions.
    template<class... Types>
    class Server: public std::vector<std::shared_ptr<Session<Types...> > >, public std::enable_shared_from_this<Server<Types...> >
    {
    public:
        Server(): std::vector<std::shared_ptr<Session<Types...> > >() {}
        virtual ~Server() = default;
    private:
    };
}//namespace app_server
#endif