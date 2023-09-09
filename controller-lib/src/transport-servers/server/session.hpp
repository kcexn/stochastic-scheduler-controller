#ifndef SESSION_HPP
#define SESSION_HPP
#include <boost/asio.hpp>
#include <functional>

// Transport layer is dependent on boost/asio.
namespace server
{
    // Sessions own a low level interface to the unerlying transport 
    // byte stream. 
    // Sessions present an iostream of bytes for higher level presentation
    // layers to interpret.
    // Sessions provide i/o operations such as read, write, and their async counterparts.
    // Sessions depend on boost/asio.hpp
    // Sessions are equal to each other iff they are each other.
    class Session: public std::enable_shared_from_this<Session>
    {
    public:
        const static std::size_t max_buflen = 65536;
        Session(): buf_(), stream_(std::ios_base::in | std::ios_base::out | std::ios_base::app), stop_signal_() {}

        inline std::array<char, max_buflen>& buf() { return buf_; }
        inline std::stringstream& stream() {return stream_; }
        inline void cancel() { stop_signal_.emit(boost::asio::cancellation_type::total); }

        inline bool operator==(const Session& other) { return this == &other; }
    protected:
        virtual void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn) =0;
        virtual void async_write(const boost::asio::const_buffer& write_buffer, const std::function<void()>& fn) =0;
        virtual void close() = 0;
        boost::asio::cancellation_signal stop_signal_;
    private:
        std::array<char, max_buflen> buf_;
        std::stringstream stream_;
    };
}
#endif