#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/asio/deferred.hpp>
#include <sys/uio.h>

#include "sctp.hpp"

namespace sctp_server{

    class sctp_stream
    {
    public:
        sctp_stream(const sctp::assoc_t& assoc_id, const sctp::sid_t& sid): assoc_id_{assoc_id},sid_{sid} {}
        inline bool operator==(const sctp_stream& other) { return (assoc_id_ == other.assoc_id_ && sid_ == other.sid_); }
    private:
        sctp::assoc_t assoc_id_;
        sctp::sid_t sid_;
    };

    class server
    {
    public:
        server(boost::asio::io_context& ioc, short port);
        ~server();
        sctp::sctp_message do_read();

        void async_read(std::function<void(const boost::system::error_code& ec)>&& f);

        void do_write(const sctp::sctp_message& msg);
    private:
        enum { 
            buflen = 1024, /* each data buffer will be of length buflen */
            cbuflen = 1024, /* the control message buffer will be of length cbuflen, 1024 should be enough for even very large numbers of control headers.*/
            num_bufs = 1
        };
        sctp::socket socket_;
        sctp::buffer bufs[num_bufs];

        //Received message.
        sctp::sctp_message rcvdmsg;

        //Stream Table
        std::vector<sctp_stream> stream_table;
        //Check if sid is in stream table;
        bool is_existing_stream(const sctp_stream& stream);
        
        // read flags.
        int sctp_recvrcvinfo = 1;
        int sctp_recvnxtinfo = 1;
        struct sctp_assoc_value sctp_future_assoc = {
            .assoc_id = SCTP_FUTURE_ASSOC,
            .assoc_value = 1
        };
        struct sctp_assoc_value sctp_reset_future_streams = {
            .assoc_id = SCTP_FUTURE_ASSOC,
            .assoc_value = SCTP_ENABLE_RESET_STREAM_REQ
        };

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