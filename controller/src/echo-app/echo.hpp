#include <boost/asio.hpp>
#include <boost/coroutine2/all.hpp>
#include "sctp.hpp"
#include "sctp-server.hpp"

namespace echo{
    class app
    {
    public:
        app(boost::asio::io_context& ioc, short port);
        typedef boost::coroutines2::coroutine<sctp::sctp_message> coro_t;
        void loop();
    private:
        sctp_server::server s_;
        coro_t::push_type echo_;
        sctp::sctp_message msg;
    };
}//echo namespace