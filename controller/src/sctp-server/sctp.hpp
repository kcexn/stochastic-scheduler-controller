#ifndef BOOST_ASIO_IP_SCTP_HPP
#define BOOST_ASIO_IP_SCTP_HPP

#include <boost/asio/detail/config.hpp>
#include <boost/asio/basic_seq_packet_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/basic_resolver.hpp>

class sctp
{
public:
  typedef boost::asio::ip::basic_endpoint<sctp> endpoint;

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