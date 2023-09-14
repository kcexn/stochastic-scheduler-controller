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
        struct stream_t {
            assoc_t assoc;
            sid_t sid;
        };

        /* Settable Socket Options */
        struct recvrcvinfo {
            static int level(const sctp& p){ return IPPROTO_SCTP; }
            static int name(const sctp& p){ return SCTP_RECVRCVINFO; }
            const void* data(const sctp& p) const { return &optval; }
            void* data(const sctp& p) { return &optval; }
            void resize(const sctp& p, const std::size_t& s){ return; }
            static std::size_t size(const sctp& p) { return sizeof(int); }
            int optval = 0;
            recvrcvinfo(const int& opt){
                optval = opt;
            }
            recvrcvinfo(){}
            int value(){ return optval; }
        };

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