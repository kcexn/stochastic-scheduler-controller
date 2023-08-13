#ifndef SCTP_SERVER_HPP
#define SCTP_SERVER_HPP
#include <thread>
#include <optional>

#include <boost/asio.hpp>
#include <sys/uio.h>

#include "sctp.hpp"
#include "../utils/uuid.hpp"

namespace sctp_server{

    class sctp_stream
    {
    public:
        sctp_stream(const sctp::assoc_t& assoc_id, const sctp::sid_t& sid): assoc_id_{assoc_id},sid_{sid} {}
        sctp::assoc_t assoc_id() { return assoc_id_; }
        sctp::sid_t sid() { return sid_; }
        inline bool operator==(const sctp_stream& other) { return (assoc_id_ == other.assoc_id_ && sid_ == other.sid_); }
        void set_tid(pthread_t t_id) {
            tid_ = {
                .id_ = t_id,
                .tid_set_ = true
            };
        }
        pthread_t get_tid(){
            if(tid_.tid_set_) {
                return tid_.id_;
            } else {
                return 0;
            }
        }
    private:
        sctp::assoc_t assoc_id_;
        sctp::sid_t sid_;
        uuid::uuid uuid_ = {};

        // TIDs should be considered opaque blocks of memory (not portable) and so
        // equality checks should be made ONLY if we are sure that the tid
        // has been set. Equality checks should only be done with man(3) pthread_equal().
        struct tid_t {
            pthread_t id_;
            bool tid_set_ = false;
        } tid_;
    };

    class server
    {
    public:
        server(boost::asio::io_context& ioc, short port);
        ~server();
        sctp::sctp_message do_read();
        void shutdown_read(sctp::endpoint remote, sctp::assoc_t assoc_id_);
        void stop();

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