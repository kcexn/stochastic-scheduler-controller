#ifndef SESSION_HPP
#define SESSION_HPP
#include <system_error>
#include <boost/asio.hpp>
#include <functional>
#define SERVER_SESSION_MAX_BUFLEN 65535

// Transport layer is dependent on boost/asio.
namespace server
{
    /*Forward declarations*/
    class Server;

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
        // const static std::size_t max_buflen = 65536;
        Session(Server& server): server_(server), stream_(std::ios_base::in | std::ios_base::out | std::ios_base::app){}
        std::array<char, SERVER_SESSION_MAX_BUFLEN>& buf() { return buf_; }
        boost::asio::const_buffer& read_buf() { return data_; }
        std::stringstream& acquire_stream(){ mtx_.lock(); return stream_; }
        void release_stream(){ mtx_.unlock(); }
        bool is_in_server();
        void cancel() { 
            stop_signal_.emit(boost::asio::cancellation_type::total);
        }
        void erase();

        virtual void async_read(std::function<void(boost::system::error_code ec, std::size_t length)> fn) =0;
        virtual void async_write(const boost::asio::const_buffer& write_buffer, const std::function<void(const std::error_code& ec)>& fn) =0;
        virtual void close() =0;

        bool operator==(const Session& other) { return this == &other; }

        virtual ~Session() = default;
        
    protected:
        boost::asio::cancellation_signal stop_signal_;
        void acquire() { mtx_.lock(); return; }
        void release() { mtx_.unlock(); return; }

    private:
        server::Server& server_;
        boost::asio::const_buffer data_;
        std::array<char, SERVER_SESSION_MAX_BUFLEN> buf_;
        std::stringstream stream_;
        std::mutex mtx_;
    };
}
#endif