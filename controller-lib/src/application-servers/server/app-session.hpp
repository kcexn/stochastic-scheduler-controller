#ifndef APP_SESSION_HPP
#define APP_SESSION_HPP
#include "../../transport-servers/server/session.hpp"
#include <iostream>

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
        Session(Server<Types...>& server): server_(server) {}
        Session(Server<Types...>& server, const std::shared_ptr<server::Session>& t_session_ptr): server_(server), t_session_(t_session_ptr) {}
        void erase(){
            server_.acquire();
            try{
                auto it = std::find(server_.begin(), server_.end(), this->shared_from_this());
                if(it != server_.end()){
                    server_.erase(it);
                }
            } catch(std::bad_weak_ptr& e){
                std::cerr << "app-session.hpp:36:shared_from_this() failed:" << e.what() << std::endl;
                throw e;
            }
            server_.release();
            return;
        }

        std::tuple<Types...> get(){ acquire_lock(); std::tuple<Types...> t = *this; release_lock(); return t; }
        void set(std::tuple<Types...> t) {acquire_lock(); std::tuple<Types...>::operator=(t); release_lock(); return; }
        std::tuple<Types...>& acquire(){ acquire_lock(); return *this;}
        void release(){ release_lock(); return;}

        virtual void read() = 0;
        virtual void write(const std::function<void()>& fn) = 0;
        virtual void write(const std::tuple<Types...>& t, const std::function<void()>& fn) = 0;
        virtual void close() = 0;

        Session& operator=(const std::tuple<Types...>& other){ std::tuple<Types...>::operator=(other); return *this; }
        Session& operator=(std::tuple<Types...>&& other){ std::tuple<Types...>::operator=(std::move(other)); return *this; }
        bool operator==(const std::shared_ptr<server::Session>& t){ return t_session_ == t;}

        virtual ~Session() = default;

    protected:
        std::shared_ptr<server::Session> t_session_;
        void acquire_lock(){ mtx_.lock(); return; }
        void release_lock(){ mtx_.unlock(); return;}

    private:
        Server<Types...>& server_;
        std::mutex mtx_;
    };
}//namespace app_server
#endif