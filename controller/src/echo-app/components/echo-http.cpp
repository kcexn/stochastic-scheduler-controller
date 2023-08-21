#include "echo-http.hpp"
namespace echo{
        HttpServer::HttpServer(std::shared_ptr<MailBox> mbox_ptr)
          : http_mbox_ptr_(mbox_ptr)
        {
            #ifdef DEBUG
            std::cout << "Echo HTTP Server Constructor!" << std::endl;
            #endif
            std::thread http_server(
                &HttpServer::start, this
            );
            tid_ = http_server.native_handle();
            http_server.detach();
        }

        void HttpServer::start(){
            // Initialize resources I might need.
            std::unique_lock<std::mutex> lk(http_mbox_ptr_->mbx_mtx, std::defer_lock);
            #ifdef DEBUG
            std::cout << "HTTP Server Started!" << std::endl;
            #endif

            // Scheduling Loop.
            while(true){
                lk.lock();
                http_mbox_ptr_->mbx_cv.wait(lk, [&]{ return (http_mbox_ptr_->msg_flag == true || http_mbox_ptr_->signal.load() != 0); });
                Http::Session session( http_mbox_ptr_->session_ptr );
                lk.unlock();
                if ( (http_mbox_ptr_->signal.load() & Signals::TERMINATE) == Signals::TERMINATE ){
                    pthread_exit(0);
                }
                std::vector<Http::Session>& sessions = http_server_.http_sessions();
                auto it = std::find(sessions.begin(), sessions.end(), session);
                if (it != sessions.end()){
                    // Do Something.
                    it->read_request();
                    #ifdef DEBUG
                    std::cout << "Session is in the HTTP Server." << std::endl;
                    #endif
                } else {
                    sessions.push_back(std::move(session));
                    http_server_.http_sessions().back().read_request();
                    #ifdef DEBUG
                    std::cout << "Session is not in the HTTP Server." << std::endl;
                    #endif
                }
                http_mbox_ptr_->msg_flag.store(false);

                #ifdef DEBUG
                std::cout << "HTTP Server Loop!" << std::endl;
                #endif
            }
        }

        void HttpServer::stop(){
            pthread_cancel(tid_);
        }

        HttpServer::~HttpServer(){
            #ifdef DEBUG
            std::cout << "Echo HTTP Server Destructor!" << std::endl;
            #endif
            stop();
        }
}