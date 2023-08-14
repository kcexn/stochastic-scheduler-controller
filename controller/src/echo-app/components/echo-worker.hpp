#ifndef ECHO_WORKER_HPP
#define ECHO_WORKER_HPP
#include "../utils/common.hpp"

namespace echo {
    class Worker{
    public:
        Worker(
            std::shared_ptr<MailBox> mbox_ptr
            );
        void start();

        #ifdef DEBUG
        ~Worker();
        #endif
    private:
        std::shared_ptr<MailBox> mbox_ptr_;
    };
}
#endif