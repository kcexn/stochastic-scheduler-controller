#ifndef BOOST_ASIO_IP_SCTP_HPP
#define BOOST_ASIO_IP_SCTP_HPP

#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/sctp.h>

#include <boost/asio/detail/config.hpp>
#include <boost/asio/basic_seq_packet_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/basic_resolver.hpp>



class sctp
{
public:
  // SCTP endpoint.
  typedef boost::asio::ip::basic_endpoint<sctp> endpoint;

  // SCTP message structure, man (2) recv.
  //   struct msghdr {
  //     void         *msg_name;       /* Optional address */
  //     socklen_t     msg_namelen;    /* Size of address */
  //     struct iovec *msg_iov;        /* Scatter/gather array */
  //     size_t        msg_iovlen;     /* # elements in msg_iov */
  //     void         *msg_control;    /* Ancillary data, see below */
  //     size_t        msg_controllen; /* Ancillary data buffer len */
  //     int           msg_flags;      /* Flags on received message */
  // };
  // msg_iov, should be populated with receive buffers.
  // msg_iovlen, should be populated with the number of receive buffers.
  // msg_control, should be populated with message controls.
  // msg_controllen, should be populated with the size of the message controls in bytes.
  // msg_flags, should be passed an integer reference.
  typedef struct msghdr envelope;

  // SCTP iovec buffers. man(3) iovec.
  typedef struct iovec buffer;

  // SCTP control message. man(2) recv.
  typedef struct cmsghdr* message_controls;

  // SCTP Control Message Buffer. man(2) recv.
  typedef void* cbuf;

  // SCTP Send Ancillary Info.
  typedef struct sctp_sndinfo sndinfo;

  // SCTP Remote Endpoint.
  // The receive and send info structs are defined in /usr/include/linux/sctp.h
  typedef struct {
    sctp::endpoint endpt;
    struct sctp_rcvinfo rcvinfo;
    struct sctp_sndinfo sndinfo;
  } sctp_rmt_endpt;

  // SCTP Received Message.
  typedef struct {
    //SCTP Remote Endpoint Keeps track of details like the remote ip address and port that the message was received from.
    //As well as all of the information in the sctp_rcvinfo struct, such as the SCTP TSN, SSN, SID, ASSOC_ID etc.
    //All of the message data is stored in the payload buffer.
    sctp::sctp_rmt_endpt rmt_endpt;
    boost::asio::const_buffer payload;
  } sctp_message;

  static sctp v4() BOOST_ASIO_NOEXCEPT
  {
    return sctp(BOOST_ASIO_OS_DEF(AF_INET));
  }


  /// Construct to represent the IPv6 UDP protocol.
  static sctp v6() BOOST_ASIO_NOEXCEPT
  {
    return sctp(BOOST_ASIO_OS_DEF(AF_INET6));
  }

  /// Obtain an identifier for the type of the protocol.
  int type() const BOOST_ASIO_NOEXCEPT
  {
    return BOOST_ASIO_OS_DEF(SOCK_SEQPACKET);
  }

  /// Obtain an identifier for the protocol.
  int protocol() const BOOST_ASIO_NOEXCEPT
  {
    return IPPROTO_SCTP;
  }

  /// Obtain an identifier for the protocol family.
  int family() const BOOST_ASIO_NOEXCEPT
  {
    return family_;
  }

  /// The SCTP socket type.
  typedef boost::asio::basic_seq_packet_socket<sctp> socket;

  /// The SCTP resolver type.
  typedef boost::asio::ip::basic_resolver<sctp> resolver;

  /// Compare two protocols for equality.
  friend bool operator==(const sctp& p1, const sctp& p2)
  {
    return p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const sctp& p1, const sctp& p2)
  {
    return p1.family_ != p2.family_;
  }

private:
  // Construct with a specific family.
  explicit sctp(int protocol_family) BOOST_ASIO_NOEXCEPT
    : family_(protocol_family)
  {
  }

  int family_;
};


#endif //BOOST_ASIO_IP_SCTP_HPP