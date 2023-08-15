#ifndef ECHO_APPLICATION_HPP
#define ECHO_APPLICATION_HPP
#include "../utils/common.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace echo{
    class App
    {
    public:
        App(
            std::vector<std::shared_ptr<MailBox> >& results,
            std::vector<echo::ExecutionContext>& context_table,
            std::shared_ptr<sctp_server::server> s_ptr
        );
        #ifdef DEBUG
        ~App();
        #endif
        // Schedule an Echo Application.
        sctp::sctp_message schedule(sctp::sctp_message& rcvdmsg);
        void deschedule(echo::ExecutionContext context);

    private:
        std::vector<std::shared_ptr<MailBox> >& results_;
        std::vector<echo::ExecutionContext>& context_table_;
        std::shared_ptr<sctp_server::server> s_ptr_;
    };
}//namespace echo
#endif