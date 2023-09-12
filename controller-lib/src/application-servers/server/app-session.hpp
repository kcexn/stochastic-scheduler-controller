#ifndef APP_SESSION_HPP
#define APP_SESSION_HPP
#include "../../transport-servers/server/session.hpp"
namespace app_server{
    /* Forward Declarations */
    template<class... Types>
    class Server;

    // The application session is a container for
    // Layer 6 Presentation layer in the OSI model.
    // The application session provides utility funcionts.
    // and machinery to:
    // 1) Bind layer 5 sessions to Application contexts.
    //      IPC and Networking prerequisites such as Domain Name Resolution
    //      should be made completely transparent to the application
    //      by the Application Session.
    // 2) Convert data between their byte stream representations and application native data types.
    //      Layer 7 applications should operate purely on native user defined data structures.
    //      The application session is responsible for converting between byte streams,
    //      (such as those streamed off a network socket, or a unix domain socket, or even a plain file descriptor),
    //      and application data structures.
    template<class... Types>
    class Session: public std::enable_shared_from_this<Session<Types...> >
    {
    public:
        Session(Server<Types...>& server): server_(server){}
        Session(Server<Types...>& server, const Types&... args): server_(server), data_(args...){}
        void erase()
        {        
            auto it = std::find(server_.begin(), server_.end(), this->shared_from_this());
            if(it != server_.end()){
                server_.erase(it);
            }
            return;
        }

        virtual void read() = 0;
        virtual void write(const std::function<void()>& fn) = 0;
        virtual void close() = 0;

    protected:
        std::tuple<Types...> data_;
        std::shared_ptr<server::Session> t_session_;

    private:
        Server<Types...>& server_;
    };
}//namespace app_server
#endif