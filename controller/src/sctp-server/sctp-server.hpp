#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP

#include "sctp.hpp"
namespace sctp_server{
    class server
    {
    public:
        server(boost::asio::io_context& ioc, short port);
        ~server();
        echo::sctp::sctp_message do_read();
        void shutdown_read(echo::sctp::endpoint remote, echo::sctp::assoc_t assoc_id_);
        void stop();
        void start();

        void async_read(std::function<void(const boost::system::error_code& ec)>&& f);
        void do_write(const echo::sctp::sctp_message& msg);
    private:
        enum { 
            buflen = 1024, /* each data buffer will be of length buflen */
            cbuflen = 1024, /* the control message buffer will be of length cbuflen, 1024 should be enough for even very large numbers of control headers.*/
            num_bufs = 1
        };
        echo::sctp::socket socket_;
        echo::sctp::buffer bufs[num_bufs];
        boost::asio::io_context& ioc_;

        //Received message.
        echo::sctp::sctp_message rcvdmsg;
        
        // read flags.
        int sctp_recvrcvinfo = 1;
        int sctp_recvnxtinfo = 1;
        struct sctp_assoc_value sctp_future_assoc = {
            SCTP_FUTURE_ASSOC,
            1
        };
        struct sctp_assoc_value sctp_reset_future_streams = {
            SCTP_FUTURE_ASSOC,
            SCTP_ENABLE_RESET_STREAM_REQ
        };

        int sctp_autoclose = 60;

    //    setsockopt option:
    //    SCTP_RESET_STREAMS
    //           This option allows the user to request the reset of
    //           incoming and/or outgoing streams.

    //           The parameter type is struct sctp_reset_streams, for
    //           writing only.  srs_assoc_id is a specified assoc_id.

    //           Require: SCTP_ENABLE_STREAM_RESET.
        typedef struct sctp_reset_streams reset_streams;

    //    setsockopt option for connecting to peers.
    //    SCTP_SOCKOPT_CONNECTX
    //           Similar to SCTP_SOCKOPT_CONNECTX_OLD, but it returns the
    //           new assoc's id.  The API sctp_connectx2() is based on this
    //           option.

    //           The parameter type is struct sockaddr[], for writing only.
    //           The new assoc's id is passed to users by the return value.
        typedef struct sockaddr_in connect_addr[];
    };

}//namespace sctp_server

#endif //SCTP_SERVER_HPP