#ifndef OWLIB_IP_SCTP_HPP
#define OWLIB_IP_SCTP_HPP
#include <boost/asio.hpp>
#include <sys/socket.h>
#include <netinet/sctp.h>

namespace transport{
namespace protocols{
    /* The SCTP protocol defined here is for the linux SCTP sockets implementation, which is 
    compliant with RFC6458 https://datatracker.ietf.org/doc/html/rfc6458 */
    class sctp
    {
    public:
        typedef boost::asio::ip::basic_endpoint<sctp> endpoint;
        typedef struct msghdr msghdr;
        typedef struct sockaddr_in sockaddr_in;
        typedef struct iovec iov;
        typedef struct cmsghdr cmsghdr;
        typedef struct sctp_sndinfo sndinfo;
        typedef struct sctp_rcvinfo rcvinfo;
        typedef sctp_assoc_t assoc_t;
        typedef std::uint16_t sid_t;
        typedef struct sctp_paddrinfo paddrinfo;
        struct stream_t {
            assoc_t assoc;
            sid_t sid;
        };

        /* Socket Options */
        struct recvrcvinfo {
            // Settable and Gettable
            static int level(const sctp& p){ return IPPROTO_SCTP; }
            static int name(const sctp& p){ return SCTP_RECVRCVINFO; }
            const void* data(const sctp& p) const { return &optval; }
            void* data(const sctp& p) { return &optval; }
            static void resize(const sctp& p, const std::size_t& s){ return; }
            static std::size_t size(const sctp& p) { return sizeof(int); }
            int optval = 0;
            recvrcvinfo(const int& opt){
                optval = opt;
            }
            recvrcvinfo(){}
            int value(){ return optval; }
        };

        // typedef struct sctp_event event;
        // struct event_opt {
        //     event optval;
        //     static int level(const sctp& p){ return IPPROTO_SCTP; }
        //     static int name(const sctp& p){ return SCTP_RECVRCVINFO; }
        //     const void* data(const sctp& p) const { return &optval; }
        //     void* data(const sctp& p) { return &optval; }
        //     static void resize(const sctp& p, const std::size_t& s){ return; }
        //     static std::size_t size(const sctp& p) { return sizeof(event); }
        //     event_opt(const event& opt){
        //         optval.se_assoc_id = opt.se_assoc_id;
        //         optval.se_type = opt.se_type;
        //         optval.se_on = opt.se_on;
        //     }
        //     event_opt(){}
        //     event value() { return optval; }
        //     // enum struct se_types{
        //     //     assoc_change = SCTP_ASSOC_CHANGE,
        //     //     peer_addr_change = SCTP_PEER_ADDR_CHANGE,
        //     //     remote_error = SCTP_REMOTE_ERROR,
        //     //     send_failed = SCTP_SEND_FAILED_EVENT,
        //     //     shutdown = SCTP_SHUTDOWN_EVENT,
        //     //     adaptation = SCTP_ADAPTATION_INDICATION,
        //     //     partial_delivery = SCTP_PARTIAL_DELIVERY_EVENT,
        //     //     authentication = SCTP_AUTHENTICATION_EVENT,
        //     //     sender_dry = SCTP_SENDER_DRY_EVENT
        //     // };
        // };

        /* SCTP Events Structs */
        typedef struct sctp_assoc_change assoc_change_event;
        /*    struct sctp_assoc_change {
                    uint16_t sac_type;
                    uint16_t sac_flags;
                    uint32_t sac_length;
                    uint16_t sac_state;
                    uint16_t sac_error;
                    uint16_t sac_outbound_streams;
                    uint16_t sac_inbound_streams;
                    sctp_assoc_t sac_assoc_id;
                    uint8_t  sac_info[];
                };
        */




        static sctp v4() noexcept
        {
            return sctp(AF_INET);
        }

        static sctp v6() noexcept
        {
            return sctp(AF_INET6);
        }

        int type() const noexcept
        {
            return SOCK_SEQPACKET;
        }

        int protocol() const noexcept
        {
            return IPPROTO_SCTP;
        }

        int family() const noexcept
        {
            return family_;
        }

        typedef boost::asio::basic_seq_packet_socket<sctp> socket;
        typedef boost::asio::basic_socket_acceptor<sctp> acceptor;

        friend bool operator==(const sctp& p1, const sctp& p2)
        {
            return p1.family_ == p2.family_;
        }

        friend bool operator!=(const sctp& p1, const sctp& p2)
        {
            return p1.family_ != p2.family_;
        }
    private:
        // Construct with a specific family.
        explicit sctp(int protocol_family) noexcept
          : family_(protocol_family)
        {}
        int family_;
    };
}
}
#endif