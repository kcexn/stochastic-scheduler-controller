#ifndef APP_SESSION_HPP
#define APP_SESSION_HPP
#include "../../transport-servers/server/session.hpp"
namespace app_server{
    /*Forward Declaration*/
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
    class Session: public std::tuple<Types...>, public std::enable_shared_from_this<Session<Types...> >
    {
    public:
        Session(Server<Types...>& server): server_(server){}
        Session(Server<Types...>& server, const std::shared_ptr<server::Session>& t_session_ptr): server_(server), t_session_(t_session_ptr) {}
        void erase()
        {        
            auto it = std::find(server_.begin(), server_.end(), this->shared_from_this());
            if(it != server_.end()){
                server_.erase(it);
            }
            return;
        }

        std::tuple<Types...> get(){ acquire_lock(); std::tuple<Types...> t = *this; release_lock(); return t; }
        void set(std::tuple<Types...> t) {acquire_lock(); std::tuple<Types...>::operator=(t); release_lock(); }

        virtual void read() = 0;
        virtual void write(const std::function<void()>& fn) = 0;
        virtual void close() = 0;



    protected:
        std::shared_ptr<server::Session> t_session_;
        void acquire_lock(){ mtx_.lock(); }
        void release_lock(){ mtx_.unlock();}

    private:
        Server<Types...>& server_;
        std::mutex mtx_;
    };
}//namespace app_server
#endif