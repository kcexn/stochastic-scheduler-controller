#ifndef HTTP_REQUESTS_HPP
#define HTTP_REQUESTS_HPP
#include <limits>
#include <string>
#include <vector>
#include <iostream>

namespace http{
    enum class HttpVerb
    {
        GET,
        POST,
        PATCH,
        PUT,
        TRACE,
        DELETE
    };

    // This is a non-exhaustive list of HTTP
    // headers. These are the only headers that 
    // we care about in our application.
    enum class HttpHeaderField
    {
        CONTENT_TYPE,
        CONTENT_LENGTH,
        ACCEPT,
        HOST,
        TRANSFER_ENCODING,
        END_OF_HEADERS,
        UNKNOWN,
        CONNECT
    };

    // This represents arbitrarily large Http Chunk Size numbers.
    class HttpBigNum: public std::vector<std::size_t>
    {
    public:
        HttpBigNum(): std::vector<std::size_t>{0} {}
        HttpBigNum(std::initializer_list<std::size_t> init): std::vector<std::size_t>(init){}
        HttpBigNum(const std::vector<std::size_t>& init): std::vector<std::size_t>(init){}
        HttpBigNum(const std::string& hex_str);

        bool operator==(const HttpBigNum& rhs);
        bool operator!=(const HttpBigNum& rhs);
        bool operator<(const HttpBigNum& rhs);
        bool operator<=(const HttpBigNum& rhs);
        bool operator>(const HttpBigNum& rhs);
        bool operator>=(const HttpBigNum& rhs);

        HttpBigNum& operator++();
        HttpBigNum operator++(int);
        HttpBigNum& operator--();
        HttpBigNum operator--(int);

        HttpBigNum& operator+=(const HttpBigNum& rhs);
        HttpBigNum& operator+=(const std::size_t& rhs);
        friend HttpBigNum operator+(HttpBigNum lhs, const HttpBigNum& rhs);
        friend HttpBigNum operator+(HttpBigNum lhs, const std::size_t& rhs);

        HttpBigNum& operator-=(const HttpBigNum& rhs);
        HttpBigNum& operator-=(const std::size_t& rhs);
        friend HttpBigNum operator-(HttpBigNum lhs, const HttpBigNum& rhs);
        friend HttpBigNum operator-(HttpBigNum lhs, const std::size_t& rhs);
       
    private:
    };
    std::ostream& operator<<(std::ostream& os, HttpBigNum& num);


    struct HttpChunk
    {
        HttpBigNum chunk_size;
        std::string chunk_data;

        // Flags and buffers to help with processing HTTP chunks.
        HttpBigNum received_bytes;
        std::string chunk_header;
        // Set to true if the chunk size has been parsed already.
        // Is set to false by default.
        bool chunk_size_found;
        bool chunk_body_start;
        // Chunk complete guards against ingesting bytes from the stream
        // that do not belong to this chunk.
        bool chunk_complete;
    };
    // Http chunks are extracted from input streams.
    std::istream& operator>>(std::istream& is, HttpChunk& chunk);
    std::ostream& operator<<(std::ostream& os, HttpChunk& chunk);
    
    struct HttpHeader
    {
        HttpHeaderField field_name;
        std::string field_value;

        //Flags and buffers to help with processing Http Headers.
        std::string buf;
        bool field_name_found;
        bool field_delimiter_found;
        bool field_value_started;
        bool field_value_ended;
        bool header_complete;
        bool not_last;
    };
    std::istream& operator>>(std::istream& is, HttpHeader& header);
    std::ostream& operator<<(std::ostream& os, HttpHeader& header);

    // This is an HTTP1.1 Request Structure.
    // It is not comprehensive, and it only partially complies with the
    // the standard. It is all of the data structures
    // necessary to support our application.
    struct HttpRequest
    {
        // These are the general purpose data structures 
        // required for an Http Request.
        HttpVerb verb;
        std::string route;
        std::vector<HttpHeader> headers;
        std::vector<HttpChunk> chunks;

        // Request processing flags and pointers.

        // Gives the index of the next http header to be processed.
        // If next_header == headers.size(), then there are 
        // no more headers to be processed.
        std::size_t next_header;
        // Gives the total number of headers in the HTTP1.1 request
        // (that we care about at least). If next_header == num_headers
        // then there are no more headers to be processed (the rest of the data is all)
        // part of the request body.
        std::size_t num_headers;

        // Gives the index of the next http chunk to be processed.
        // If next_chunk == chunks.size(), then there is no more 
        // data to be processed.
        std::size_t next_chunk;
        // Gives the total number of chunks in the HTTP1.1 request.
        // If next_chunk == num_chunks then no more data should
        // be processed (the request body is complete, any further data that)
        // arrives in the stream should be disregarded.
        std::size_t num_chunks;

        // If this flag is true, then Content-Length header field must be present.
        // Otherwise chunked transfer encoding is assumed.
        // By default, chunked transfer encoding is assumed.
        bool not_chunked_transfer;
    };
}
#endif