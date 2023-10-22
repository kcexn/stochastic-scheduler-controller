#include "http-requests.hpp"
#include <charconv>
#include <limits>
#include <ios>
#include <ostream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace http
{
    HttpBigNum::HttpBigNum(HttpBigNum::Hex, const std::string& hex_str) {
        const std::size_t max_str_width = 2*sizeof(std::size_t);
        const std::size_t hexstr_len = hex_str.size();
        std::vector<std::string> hex_str_partitions((hex_str.size()/max_str_width) + 1);
        std::size_t offset = 0;
        for(offset = 0; offset < hex_str_partitions.size()-1; ++offset){
            // hex string partitions in here will be of full width.
            hex_str_partitions[offset].reserve(max_str_width);
            hex_str_partitions[offset].insert(hex_str_partitions[offset].end(), hex_str.end()-(offset+1)*max_str_width, hex_str.end()-(offset)*max_str_width);
        }
        // Here the size is less than the full width and so the sizing must be dynamic.
        std::ptrdiff_t len = (hex_str.end() - (offset)*max_str_width) - hex_str.begin();
        hex_str_partitions[offset].reserve(len);
        hex_str_partitions[offset].insert(hex_str_partitions[offset].end(), hex_str.begin(), hex_str.end()-(offset)*max_str_width);

        for(auto it=hex_str_partitions.rbegin(); it != hex_str_partitions.rend(); ++it){
            std::size_t num;
            std::from_chars_result res = std::from_chars(it->data(), it->data()+it->size(), num, 16);
            if(res.ec != std::errc{}){
                std::cerr << "http-requests.cpp:31:std::from_chars failed:" << std::make_error_code(std::errc(res.ec)).message() << ":value:" << *it << std::endl;
                throw "char conversion failed.";
            }
            push_back(num);
        }
    }

    HttpBigNum::HttpBigNum(HttpBigNum::Dec, const std::string& dec_str): std::vector<std::size_t>{0}{
        std::from_chars_result res;
        std::size_t val;
        for(std::size_t i = 0; i < dec_str.size(); ++i){
            res = std::from_chars(&(dec_str[i]),&(dec_str[i])+1, val, 10);
            if(res.ec != std::errc{}){
                std::cerr << "http-requests.cpp:43:std::from_chars failed:" << std::make_error_code(std::errc(res.ec)).message() << std::endl;
                throw "Char conversion failed.";
            }
            HttpBigNum tmp = *this;
            for (std::size_t j = 0; j < 9; ++j){
                // multiply by 10.
                *this += tmp;
            }
            *this += val;
        }
    }

    bool HttpBigNum::operator==(const HttpBigNum& rhs){
        if(size() != rhs.size()){
            return false;
        } else {
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] != rhs[i]){
                    return false;
                }
            }
            return true;
        }
    }

    bool HttpBigNum::operator!=(const HttpBigNum& rhs){
        if(size() != rhs.size()){
            return true;
        } else {
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] != rhs[i]){
                    return true;
                }
            }
            return false;
        }
    }


    bool HttpBigNum::operator<(const HttpBigNum& rhs){
        if(rhs.size() > size()){
            return true;
        } else if (rhs.size() < size()){
            return false;
        } else {
            // return lexicographic sort iff the vectors are the same
            // length.
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] > rhs[i]){
                    return false;
                } else if ((*this)[i] < rhs[i]){
                    return true;
                } // Otherwise the numbers are equal.
            }
            // If everything is equal return false.
            return false;
        }
    }

    bool HttpBigNum::operator<=(const HttpBigNum& rhs){
        if(rhs.size() > size()){
            return true;
        } else if (rhs.size() < size()){
            return false;
        } else {
            // return lexicographic sort iff the vectors are the same
            // length.
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] > rhs[i]){
                    return false;
                } else if ((*this)[i] < rhs[i]){
                    return true;
                } // otherwise the numbers are equal.
            }
            // If everything is equal return true.
            return true;
        }
    }

    bool HttpBigNum::operator>(const HttpBigNum& rhs){
        if(rhs.size() > size()){
            return false;
        } else if (rhs.size() < size()){
            return true;
        } else {
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] < rhs[i]){
                    return false;
                } else if ((*this)[i] > rhs[i]){
                    return true;
                }
            }
            return false;
        }
    }

    bool HttpBigNum::operator>=(const HttpBigNum& rhs){
        if(rhs.size() > size()){
            return false;
        } else if (rhs.size() < size()){
            return true;
        } else {
            for(std::size_t i=0; i < size(); ++i){
                if((*this)[i] < rhs[i]){
                    return false;
                } else if ((*this)[i] > rhs[i]){
                    return true;
                }
            }
            return true;
        }
    }

    HttpBigNum& HttpBigNum::operator++(){
        // start from the least significant bit, add 1 and handle the carry.
        std::size_t carry = 1;
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        auto it = rbegin();
        do{
            if(*it == max){
                *it = 0;
            } else {
                ++(*it);
                carry = 0;
            }
            ++it;
        } while(it != rend() && carry > 0);
        if(it == rend() && carry >0){
            insert(begin(),1);
        }
        return *this;
    }

    HttpBigNum HttpBigNum::operator++(int){
        HttpBigNum tmp{};
        tmp = ++(*this);
        return tmp;
    }

    HttpBigNum& HttpBigNum::operator--(){
        std::size_t carry = 1;
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        auto it = rbegin();
        do{
            if(carry == 1 && (*it ==0)){
                *it = max;
            } else {
                *it -= carry;
                carry = 0;
            }
            ++it;
        } while (it != rend() && carry == 1);
        if(it == rend() && front() == 0){
            // remove leading zero.
            erase(begin());
        }
        return *this;
    }

    HttpBigNum HttpBigNum::operator--(int){
        HttpBigNum tmp{};
        tmp = --(*this);
        return tmp;
    }

    HttpBigNum& HttpBigNum::operator+=(const HttpBigNum& rhs){
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        std::size_t carry = 0;
        std::size_t offset = 0;
        for(offset = 0; offset < rhs.size(); ++offset){
            std::size_t tmp = 0;
            auto it1 = rhs.cend() - offset -1;
            // Check that *it1 doesn't overflow
            if(carry > 0 && carry > max - *it1){
                tmp = *it1 - (max - carry + 1);
                carry = 1;
            } else {
                tmp = *it1 + carry;
                carry = 0;
            }
            // Select the number in the correct position.
            auto it0 = end() - offset -1;

            if(offset >= size()){
                // Handle the case where rhs is much longer than the lhs.
                insert(begin(),tmp);
            } else if(tmp > 0 && tmp > max - *it0){
                *it0 = *it0 - (max - tmp + 1);
                carry = 1;
            } else {
                *it0 += tmp;
            }
        }
        if(carry > 0){
            insert(begin(),carry);
        }
        return *this;
    }

    HttpBigNum& HttpBigNum::operator+=(const std::size_t& rhs){
        HttpBigNum other{rhs};
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        std::size_t carry = 0;
        std::size_t offset = 0;
        for(offset = 0; offset < other.size(); ++offset){
            std::size_t tmp = 0;
            // the iterators need to be declared inside the loop
            // since we need to be able to handle the scenario
            // where the lhs and the rhs do not have the same lengths.
            auto it1 = other.cend() - offset -1;
            // Check that *it1 doesn't overflow
            if(carry > 0 && carry > max - *it1){
                tmp = *it1 - (max - carry + 1);
                carry = 1;
            } else {
                tmp = *it1 + carry;
                carry = 0;
            }
            // Select the number in the correct position.
            auto it0 = end() - offset -1;

            if(offset >= size()){
                // Handle the case where rhs is much longer than the lhs.
                insert(begin(),tmp);
            } else if(tmp > 0 && tmp > max - *it0){
                *it0 = *it0 - (max - tmp + 1);
                carry = 1;
            } else {
                *it0 += tmp;
            }
        }
        if(carry > 0){
            insert(begin(),carry);
        }
        return *this;
    }

    HttpBigNum operator+(HttpBigNum lhs, const HttpBigNum& rhs){
        lhs += rhs;
        return lhs;
    }

    HttpBigNum operator+(HttpBigNum lhs, const std::size_t& rhs){
        lhs += rhs;
        return lhs;
    }

    HttpBigNum& HttpBigNum::operator-=(const HttpBigNum& rhs){
        if(*this <= rhs){
            // For the HTTP use case:
            // if subtraction underflows we set the number
            // to be equal to 0.
            *this = {0};
            return *this;
        }
        std::size_t carry = 0;
        std::size_t offset = 0;
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        for(offset = 0; offset < rhs.size(); ++offset){
            // the iterators need to be declared inside the loop
            // since we need to be able to handle the scenario
            // where the lhs and the rhs do not have the same lengths.
            auto it1 = rhs.cend() - offset -1;
            auto it0 = end() - offset -1;
            if(carry == 0){
                if(*it1 > *it0){
                    *it0 = max - (*it1 - *it0);
                    carry = 1;
                } else {
                    *it0 = (*it0 - *it1);
                }
            } else {
                if(*it1 >= *it0){
                    *it0 = max - (*it1 - *it0);
                } else {
                    *it0 = (*it0 - *it1 - carry);
                    carry = 0;
                }
            }
        }
        // subtract the last carry.
        while(carry > 0 && offset < size()){
            auto it0 = end() - offset - 1;
            if(*it0 == 0){
                *it0 = max;
            } else {
                *it0 -= carry;
                carry = 0;
            }
            ++offset;
        }

        // Find and remove the leading zeros.
        auto end = begin();
        auto start = end;
        while(*end == 0){
            ++end;
        }
        erase(start,end);
        return *this;        
    }

    HttpBigNum& HttpBigNum::operator-=(const std::size_t& rhs){
        HttpBigNum tmp{rhs};
        if(*this <= tmp){
            // For the HTTP use case:
            // if subtraction overflows we set the number
            // to be equal to 0.
            *this = {0};
            return *this;
        }
        std::size_t carry = 0;
        std::size_t offset = 0;
        const std::size_t max = std::numeric_limits<std::size_t>::max();
        for(offset = 0; offset < tmp.size(); ++offset){
            // the iterators need to be declared inside the loop
            // since we need to be able to handle the scenario
            // where the lhs and the tmp do not have the same lengths.
            auto it1 = tmp.cend() - offset -1;
            auto it0 = end() - offset -1;
            if(carry == 0){
                if(*it1 > *it0){
                    *it0 = max - (*it1 - *it0);
                    carry = 1;
                } else {
                    *it0 = (*it0 - *it1);
                }
            } else {
                if(*it1 >= *it0){
                    *it0 = max - (*it1 - *it0);
                } else {
                    *it0 = (*it0 - *it1 - carry);
                    carry = 0;
                }
            }
        }
        // subtract the last carry.
        while(carry > 0 && offset < size()){
            auto it0 = end() - offset - 1;
            if(*it0 == 0){
                *it0 = max;
            } else {
                *it0 -= carry;
                carry = 0;
            }
            ++offset;
        }
        // Find the leading 0s.
        auto end = begin();
        auto start = end;
        while(*end == 0){
            ++end;
        }
        erase(start,end);
        return *this; 
    }

    HttpBigNum operator-(HttpBigNum lhs, const HttpBigNum& rhs){
        lhs -= rhs;
        return lhs;
    }

    HttpBigNum operator-(HttpBigNum lhs, const std::size_t& rhs){
        lhs -= rhs;
        return lhs;
    }

    std::ostream& operator<<(std::ostream& os, const HttpBigNum& rhs){
        auto it = rhs.begin();
        os << std::hex << *it;
        ++it;
        while(it != rhs.end()){
            os << std::setfill('0') << std::setw(2*sizeof(std::size_t)) << std::hex << *it;
            ++it;
        }
        return os;
    }

    std::istream& operator>>(std::istream& is, HttpChunk& chunk){
        // Get the stream locale for parsing.
        // std::locale loc = is.getloc();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        char c;
        // While there are bytes available in the input stream, keep parsing.
        while(bytes_available > 0 && !chunk.chunk_complete){
            c = is.rdbuf()->sbumpc();
            --bytes_available;
            if(!chunk.chunk_size_started){
                // Skip all of the leading whitespace.
                if(!std::isspace(c)){
                    chunk.chunk_size_started = true;
                    chunk.chunk_header.push_back(c);
                }
            } else if(!chunk.chunk_size_found){
                if(!std::isspace(c)){
                    chunk.chunk_header.push_back(c);
                } else {
                    chunk.chunk_size_found = true;
                    chunk.chunk_size = HttpBigNum(HttpBigNum::hex, chunk.chunk_header);
                }
            } else if(!chunk.chunk_body_start){
                // Skip all of the characters until the next newline characters.
                if(c == '\n'){
                    chunk.chunk_body_start = true;
                    chunk.received_bytes = {0};
                }
            } else if(!chunk.chunk_body_finished && chunk.received_bytes < chunk.chunk_size){
                chunk.chunk_data.push_back(c);
                ++(chunk.received_bytes);
            } else {
                // For chunked transfer, there will be another \r\n after the
                // chunk data to delimit the beginning of the next chunk.
                // If the transfer encoding is not chunked, then there will be no
                // \r\n and the chunk will not be marked complete.
                // Not marking the chunk complete will prevent the parser from
                // constructing and back emplacing a new chunk, and beginning
                // the search for a new chunk header line.
                if(c == '\n'){
                    chunk.chunk_complete = true;
                }
            }
            if(bytes_available == 0){
                bytes_available = is.rdbuf()->in_avail();
            }
        }
        return is;
    }

    std::ostream& operator<<(std::ostream& os, const HttpChunk& chunk){
        os << chunk.chunk_size << "\r\n"
           << chunk.chunk_data << "\r\n";
        return os;
    }

    std::istream& operator>>(std::istream& is, HttpHeader& header){
        // Get the stream locale for parsing.
        char cur;
        // std::locale loc = is.getloc();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        // While there are bytes available in the input stream, keep parsing.
        while(bytes_available > 0 && !header.header_complete){
            cur = is.rdbuf()->sbumpc();
            --bytes_available;
            if(!header.field_name_found){
                //Seek until either a non-white space character, or a new line.
                if(!header.not_last){
                    if(cur == '\n'){
                        // A new line was found without finding any
                        // non-whitespace characters. Therefore this is the last header.
                        // Mark it as the last header, and break.
                        header.field_name_found = true;
                        header.field_name = HttpHeaderField::END_OF_HEADERS;
                        header.header_complete = true;
                        break;
                    } else if(!std::isspace(cur)){
                        // a non-whitespace character was found;
                        // therefore; this is not the last header.
                        header.not_last = true;
                        // start pushing characters onto the buffer.
                        header.buf.push_back(cur);
                    }
                } else {
                    // A non-whitespace character was found.
                    // This is a valid header, but we still haven't found
                    // the header field name.

                    // Push all of the subsequent non white-space and non-delimter ':'
                    // characters onto the buffer.
                    if(!(std::isspace(cur) || cur == ':')){
                        header.buf.push_back(cur);
                    } else {
                        // White space or the delimiter ':' has been found.

                        // If the delimiter ':' has been found, then mark it.
                        if(cur == ':'){
                            header.field_delimiter_found = true;
                        }
                        // Else we have found white space between the delimiter
                        // and the header field name. This is technically
                        // not allowed by the new RFC, RFC 9112, as 
                        // incorrect handling of this white space has lead to security faults
                        // in the past.
                        
                        header.field_name_found = true;

                        // Normalize the header field name to upper case values.
                        // This way the header fields become case insensitive.
                        std::transform(header.buf.cbegin(), header.buf.cend(), header.buf.begin(), [](unsigned char c){ return std::toupper(c); });
                        // Set the header field name.
                        if(header.buf == "CONTENT-TYPE"){
                            header.field_name = HttpHeaderField::CONTENT_TYPE;
                        } else if (header.buf == "CONTENT-LENGTH"){
                            header.field_name = HttpHeaderField::CONTENT_LENGTH;
                        } else if (header.buf == "ACCEPT"){
                            header.field_name = HttpHeaderField::ACCEPT;
                        } else if (header.buf == "HOST"){
                            header.field_name = HttpHeaderField::HOST;
                        } else if (header.buf == "TRANSFER-ENCODING"){
                            header.field_name = HttpHeaderField::TRANSFER_ENCODING;
                        } else if (header.buf == "CONNECTION") {
                            header.field_name = HttpHeaderField::CONNECTION;
                        } else {
                            header.field_name = HttpHeaderField::UNKNOWN;
                        }
                    }
                }
            } else {
                // header field name has been found.
                // First, if the header field delimiter ':' has not been found yet, we need to seek for it.
                if (!header.field_delimiter_found && cur == ':'){
                    // We have found the delimiter.
                    header.field_delimiter_found = true;
                } else {
                    // Seek for the first non-white space character.
                    // OR a new line character, which marks the end of the header field value (i.e.; empty header field).
                    if(cur == '\n'){
                        // The newline character marks the end of the header field value.
                        // Technically, there is an exception for the message/http media type that delimited line folding is allowed.
                        // However for the purposes of my application, I am only planning on implementing the application/json media type.
                        header.header_complete = true;
                        break;
                    } else if(!std::isspace(cur) && !header.field_value_started){
                        // The first non-whitespace character marks the beginning of the field falues.
                        header.field_value_started = true;
                        header.field_value.push_back(cur);
                    } else if(header.field_value_started && !header.field_value_ended){
                        // The first non-white space character has been found,
                        // that means that all visible ASCII character + spaces + tabs 
                        // are part of the header field value, otherwise, the field 
                        // value has finished.
                        if(!std::isspace(cur) || cur == ' ' || cur == '\t'){
                            header.field_value.push_back(cur);
                        } else {
                            header.field_value_ended = true;
                        }
                    }
                }
            }
            bytes_available = is.rdbuf()->in_avail();
        }
        return is;
    }

    std::ostream& operator<<(std::ostream& os, const HttpHeader& header){
        switch(header.field_name)
        {
            case HttpHeaderField::CONTENT_TYPE:
                os << "Content-Type: ";
                break;
            case HttpHeaderField::CONTENT_LENGTH:
                os << "Content-Length: ";
                break;
            case HttpHeaderField::ACCEPT:
                os << "Accept: ";
                break;
            case HttpHeaderField::HOST:
                os << "Host: ";
                break;
            case HttpHeaderField::TRANSFER_ENCODING:
                os << "Transfer-Encoding: ";
                break;
            case HttpHeaderField::CONNECTION:
                os << "Connection: ";
                break;
            default:
                return os;
        }
        os << header.field_value << "\r\n";
        return os;
    }

    std::istream& operator>>(std::istream& is, HttpRequest& req){
        // Get the stream locale for parsing.
        // std::locale loc = is.getloc();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        char c;
        if(req.num_headers == 0 || req.num_chunks == 0 || req.verb == HttpVerb::UNKNOWN){
            // managmeent things we need to do if this is a 
            // brand new 0 initialized request.

            // There has to be at least one header
            // and there has to be at least one chunk.
            // (the header can be empty i.e. "\r\n").
            // (the chunk can also be empty i.e. "0\r\n\r\n").
            req.num_headers = 1;
            req.num_chunks = 1;
            req.headers.emplace_back();
            HttpChunk new_chunk = {
                {0},"",{0},"",false,false,false,false,false
            };
            req.chunks.push_back(
                new_chunk
            );

            // For clarity we will explicitly 0 initialize
            // the next indices.
            req.next_header = 0;
            req.next_chunk = 0;
        }
        // While there are bytes available in the input stream, keep parsing.
        // It is the programmers responsibility to ensure that the 
        // invariant that headers are fully parsed when next_header == num_headers
        // and that chunks are fully parsed when next_chunk == num_chunks.
        while(bytes_available > 0 && (req.num_headers != req.next_header || req.num_chunks != req.next_chunk)){
            if(!req.http_request_line_complete){
                // First parse the request line, which has a format of:
                // VERB ROUTE HTTP/VERSION\r\n
                while(bytes_available > 0 && !req.http_request_line_complete){
                    c = is.rdbuf()->sbumpc();
                    --bytes_available;
                    if(!req.verb_started){
                        // Ignore all leading white space
                        // until we find a non-white space
                        // character that matches the first case-sensitive character of one of the HTTP verbs.
                        // G, P, T, C, D
                        // Hopefully, the white space check, in addition to the specific character check
                        // will limit the probability that a capital letter in a spurious data stream
                        // creates a memory leak in the application.
                        if(!std::isspace(c) && (c == 'G' || c == 'P' || c == 'T' || c == 'D' || c == 'C')){
                            req.verb_started = true;
                            req.verb_buf.push_back(c);
                        } else {
                            return is;
                        }
                    } else if (!req.verb_finished){
                        // append all non-white space
                        // characters until we find the first white space character.
                        if(!std::isspace(c)){
                            req.verb_buf.push_back(c);
                        } else {
                            if(req.verb_buf == "GET"){
                                req.verb = HttpVerb::GET;
                            } else if (req.verb_buf == "POST"){
                                req.verb = HttpVerb::POST;
                            } else if (req.verb_buf == "PATCH"){
                                req.verb = HttpVerb::PATCH;
                            } else if (req.verb_buf == "PUT"){
                                req.verb = HttpVerb::PUT;
                            } else if (req.verb_buf == "TRACE"){
                                req.verb = HttpVerb::TRACE;
                            } else if (req.verb_buf == "DELETE"){
                                req.verb = HttpVerb::DELETE;
                            } else if (req.verb_buf == "CONNECT"){
                                req.verb = HttpVerb::CONNECT;
                            }
                            req.verb_finished = true;
                        }
                    } else if (!req.route_started){
                        // Seek through white space until we find 
                        // the first non-white space character.
                        if(!std::isspace(c)){
                            req.route_started = true;
                            req.route.push_back(c);
                        }
                    } else if (!req.route_finished){
                        if(!std::isspace(c)){
                            req.route.push_back(c);
                        } else {
                            req.route_finished = true;
                        }
                    } else if (req.find_version_state < HttpRequest::max_find_state){
                        // Seek through white space until we find
                        // the string 'HTTP/'
                        switch(req.find_version_state)
                        {
                            case 0:
                                if(c == 'H'){
                                    req.find_version_state = 1;
                                }
                                break;
                            case 1:
                                if(c == 'T'){
                                    req.find_version_state = 2;
                                } else {
                                    req.find_version_state = 0;
                                }
                                break;
                            case 2:
                                if(c == 'T'){
                                    req.find_version_state = 3;
                                } else {
                                    req.find_version_state = 0;
                                }
                                break;
                            case 3:
                                if(c == 'P'){
                                    req.find_version_state = 4;
                                } else {
                                    req.find_version_state = 0;
                                }
                                break;
                            case 4:
                                if(c == '/'){
                                    req.find_version_state = 5;
                                } else {
                                    req.find_version_state = 0;
                                }
                                break;
                        }
                    } else if (!req.version_finished){
                        // We have found the string 'HTTP/'. Now everything until the subsequent white
                        // space character is part of the version string.
                        if(!std::isspace(c)){
                            req.version_buf.push_back(c);
                        } else {
                            if(req.version_buf == "1.1"){
                                req.version = HttpVersion::V1_1;
                            } else if (req.version_buf == "1.0"){
                                req.version = HttpVersion::V1;
                            } else if (req.version_buf == "2"){
                                req.version = HttpVersion::V2;
                            } else if (req.version_buf == "3"){
                                req.version = HttpVersion::V3;
                            } else if (req.version_buf == "0.9"){
                                req.version = HttpVersion::V0_9;
                            }
                            req.version_finished = true;
                        }
                    } else {
                        // Seek to the end of the line.
                        if(c == '\n'){
                            req.http_request_line_complete = true;
                        }
                    }
                }
            } else if (req.num_headers != req.next_header){
                HttpHeader& next_header = req.headers[req.next_header];
                // Parse the next header.
                is >> next_header;
                if(next_header.header_complete){
                    // If the header is complete we need to
                    // do some request state management.
                    // In particular, we need to check to see if
                    // there is a Content-Length header,
                    // which toggles the way in which chunks are parsed.
                    if(next_header.field_name == HttpHeaderField::CONTENT_LENGTH){
                        req.not_chunked_transfer = true;
                    }
                    if(next_header.not_last){
                        ++(req.num_headers);
                        req.headers.emplace_back();
                    }
                    ++(req.next_header);
                }
            } else if (req.num_chunks != req.next_chunk){
                // We need to toggle for the case where a Content-Length header is present.
                HttpChunk& next_chunk = req.chunks[req.next_chunk];
                if(req.not_chunked_transfer){
                    // If it is not a chunked transfer, then we assign Content-Length to
                    // the chunk size. And set the chunk_size_found, and chunk_body_start flags.
                    if(!next_chunk.chunk_size_found && !next_chunk.chunk_body_start){
                        auto it = std::find_if(req.headers.begin(), req.headers.end(), [](auto& header){
                            return header.field_name == HttpHeaderField::CONTENT_LENGTH;
                        });
                        next_chunk.chunk_size = HttpBigNum(HttpBigNum::dec, it->field_value);
                        next_chunk.chunk_size_started = true;
                        next_chunk.chunk_size_found = true;
                        next_chunk.chunk_body_start = true;
                    }
                    is >> next_chunk;
                    if(next_chunk.chunk_size == next_chunk.received_bytes){
                        // Finish parsing.
                        req.next_chunk = req.num_chunks;
                    }
                } else {
                    is >> next_chunk;
                    if(next_chunk.chunk_complete){
                        if(next_chunk.chunk_size != HttpBigNum{0}){
                            ++(req.num_chunks);
                            HttpChunk new_chunk = {
                                {0},"",{0},"",false,false,false,false,false
                            };
                            req.chunks.push_back(new_chunk);
                        }
                        ++(req.next_chunk);
                    }
                }
            }
            bytes_available = is.rdbuf()->in_avail();
        }
        return is;
    }

    std::ostream& operator<<(std::ostream& os, const HttpRequest& req){
        if(!req.http_request_line_complete){
            switch(req.verb){
                case HttpVerb::GET:
                    os << "GET ";
                    break;
                case HttpVerb::POST:
                    os << "POST ";
                    break;
                case HttpVerb::PATCH:
                    os << "PATCH ";
                    break;
                case HttpVerb::PUT:
                    os << "PUT ";
                    break;
                case HttpVerb::TRACE:
                    os << "TRACE ";
                    break;
                case HttpVerb::DELETE:
                    os << "DELETE ";
                    break;
                case HttpVerb::CONNECT:
                    os << "CONNECT ";
                    break;
            }
            os << req.route << " HTTP/";
            switch(req.version)
            {
                case HttpVersion::V1:
                    os << "1.0\r\n";
                    break;
                case HttpVersion::V1_1:
                    os << "1.1\r\n";
                    break;
                case HttpVersion::V2:
                    os << "2\r\n";
                    break;
                case HttpVersion::V3:
                    os << "3\r\n";
                    break;
                case HttpVersion::V0_9:
                    os << "0.9\r\n";
                    break;
            }
        }
        std::size_t num_headers = req.headers.size();
        if (req.next_header != num_headers){
            for(std::size_t i = req.next_header; i < num_headers; ++i){
                const http::HttpHeader& header = req.headers[i];
                if(header.field_name == HttpHeaderField::END_OF_HEADERS){
                    os << "\r\n";
                } else {
                    os << header;
                }
            }
        }
        std::size_t num_chunks = req.chunks.size();
        if(req.verb != HttpVerb::GET && req.verb != HttpVerb::DELETE && req.verb != HttpVerb::TRACE && req.next_chunk != num_chunks){
            auto it = std::find_if(req.headers.begin(), req.headers.end(), [](auto& header){
                return header.field_name == HttpHeaderField::CONTENT_LENGTH;
            });
            if(it != req.headers.end()){
                os << req.chunks[0].chunk_data;
            } else {
                for(std::size_t i = req.next_chunk; i < num_chunks; ++i ){
                    const http::HttpChunk& chunk = req.chunks[i];
                    os << chunk;
                }
            }
        }
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const HttpResponse& res){
        if(!res.status_line_finished){
            switch(res.version)
            {
                case HttpVersion::V1:
                    os << "HTTP/1.0 ";
                    break;
                case HttpVersion::V1_1:
                    os << "HTTP/1.1 ";
                    break;
                case HttpVersion::V2:
                    os << "HTTP/2 ";
                    break;
                case HttpVersion::V3:
                    os << "HTTP/3 ";
                    break;
                case HttpVersion::V0_9:
                    os << "HTTP/0.9 ";
                    break;
            }
            switch(res.status)
            {
                case HttpStatus::OK:
                    os << "200 OK\r\n";
                    break;
                case HttpStatus::NOT_FOUND:
                    os << "404 Not Found\r\n";
                    break;
                case HttpStatus::CONFLICT:
                    os << "409 Conflict\r\n";
                    break;
                case HttpStatus::METHOD_NOT_ALLOWED:
                    os << "405 Method Not Allowed\r\n";
                    break;
                case HttpStatus::INTERNAL_SERVER_ERROR:
                    os << "500 Internal Server Error\r\n";
                    break;
                case HttpStatus::CREATED:
                    os << "201 Created\r\n";
                    break;
                case HttpStatus::ACCEPTED:
                    os << "202 Accepted\r\n";
                    break;
            }
        }
        std::size_t num_headers = res.headers.size();
        if(res.next_header != num_headers){
            for(std::size_t i = res.next_header; i < num_headers; ++i){
                const http::HttpHeader& header = res.headers[i];
                if(header.field_name == HttpHeaderField::END_OF_HEADERS){
                    os << "\r\n";
                } else {
                    os << header;
                }
            }
        }

        std::size_t num_chunks = res.chunks.size();
        if(res.next_chunk != num_chunks){
            auto it = std::find_if(res.headers.begin(), res.headers.end(), [](auto& header){
                return header.field_name == HttpHeaderField::CONTENT_LENGTH;
            });
            if(it != res.headers.end()){
                os << res.chunks[0].chunk_data;
            } else {
                for(std::size_t i = res.next_chunk; i < num_chunks; ++i){
                    const http::HttpChunk& chunk = res.chunks[i];
                    os << chunk;
                }
            }           
        }
        return os;
    }

    std::istream& operator>>(std::istream& is, HttpResponse& res){
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        char c;
        if(res.num_headers == 0 || res.num_chunks == 0 || res.find_version_state == 0){
            // managmeent things we need to do if this is a 
            // brand new 0 initialized response.

            // There has to be at least one header
            // and there has to be at least one chunk.
            // (the header can be empty i.e. "\r\n").
            // (the chunk can also be empty i.e. "0\r\n\r\n").
            res.num_headers = 1;
            res.num_chunks = 1;
            res.headers.emplace_back();
            HttpChunk new_chunk = {
                {0},"",{0},"",false,false,false,false,false
            };
            res.chunks.push_back(new_chunk);

            // For clarity we will explicitly 0 initialize
            // the next indices.
            res.next_header = 0;
            res.next_chunk = 0;
        }
        // While there are bytes available in the input stream, keep parsing.
        // It is the programmers responsibility to ensure that the 
        // invariant that headers are fully parsed when next_header == num_headers
        // and that chunks are fully parsed when next_chunk == num_chunks.
        while(bytes_available > 0 && (res.num_headers != res.next_header || res.num_chunks != res.next_chunk)){
            if(!res.status_line_finished){
                // First parse the status line, which has a format of:
                // HTTP/VERSION STATUS_CODE STATUS_MESSAGE\r\n
                while(bytes_available > 0 && !res.status_line_finished){
                    c = is.rdbuf()->sbumpc();
                    --bytes_available;
                    if(res.find_version_state < res.max_find_state){
                        // Search for the HTTP VERSION.
                        switch(res.find_version_state)
                        {
                            case 0:
                                if(c == 'H'){
                                    res.find_version_state = 1;
                                }
                                break;
                            case 1:
                                if(c == 'T'){
                                    res.find_version_state = 2;
                                } else {
                                    res.find_version_state = 0;
                                }
                                break;
                            case 2:
                                if(c == 'T'){
                                    res.find_version_state = 3;
                                } else {
                                    res.find_version_state = 0;
                                }
                                break;
                            case 3:
                                if(c == 'P'){
                                    res.find_version_state = 4;
                                } else {
                                    res.find_version_state = 0;
                                }
                                break;
                            case 4:
                                if(c == '/'){
                                    res.find_version_state = 5;
                                } else {
                                    res.find_version_state = 0;
                                }
                                break;
                        }
                    } else if (!res.version_finished){
                        if(!std::isspace(c)){
                            // Append all of the subsequent non-whitespace characters into the version buffer.
                            res.version_buf.push_back(c);
                        } else {
                            // At the first whitespace character set the http version.
                            if(res.version_buf == "0.9"){
                                res.version = HttpVersion::V0_9;
                            } else if (res.version_buf == "1.0"){
                                res.version = HttpVersion::V1;
                            } else if (res.version_buf == "1.1"){
                                res.version = HttpVersion::V1_1;
                            } else if (res.version_buf == "2"){
                                res.version = HttpVersion::V2;
                            } else if (res.version_buf == "3"){
                                res.version = HttpVersion::V3;
                            }
                            res.version_finished = true;
                        }
                    } else if (!res.status_started){
                        if(!std::isspace(c)){
                            // Seek to the first non-whitespace character.
                            res.status_buf.push_back(c);
                            res.status_started = true;
                        }
                    } else if (!res.status_finished){
                        if(!std::isspace(c)){
                            // Append all of the subsequent non-whitespace characters
                            // until the first whitespace character.
                            res.status_buf.push_back(c);
                        } else {
                            if(res.status_buf == "200"){
                                res.status = HttpStatus::OK;
                            } else if (res.status_buf == "204"){
                                res.status = HttpStatus::NO_CONTENT;
                            } else if (res.status_buf == "404"){
                                res.status = HttpStatus::NOT_FOUND;
                            } else if (res.status_buf == "409"){
                                res.status = HttpStatus::CONFLICT;
                            } else if (res.status_buf == "405"){
                                res.status = HttpStatus::METHOD_NOT_ALLOWED;
                            } else if (res.status_buf == "500"){
                                res.status = HttpStatus::INTERNAL_SERVER_ERROR;
                            } else if (res.status_buf == "201"){
                                res.status = HttpStatus::CREATED;
                            } else if (res.status_buf == "202"){
                                res.status = HttpStatus::ACCEPTED;
                            }
                            res.status_finished = true;
                        }
                    } else if (!res.status_line_finished){
                        if(c == '\n'){
                            // Seek to the newline character.
                            res.status_line_finished = true;
                        }
                    }
                }
            } else if (res.num_headers != res.next_header){
                HttpHeader& next_header = res.headers[res.next_header];
                // Parse the next header.
                is >> next_header;
                if(next_header.header_complete){
                    // If the header is complete we need to
                    // do some request state management.
                    // In particular, we need to check to see if
                    // there is a Content-Length header,
                    // which toggles the way in which chunks are parsed.
                    if(next_header.field_name == HttpHeaderField::CONTENT_LENGTH){
                        res.not_chunked_transfer = true;
                    }
                    if(next_header.not_last){
                        ++(res.num_headers);
                        res.headers.emplace_back();
                    }
                    ++(res.next_header);
                }
            } else if (res.num_chunks != res.next_chunk){
                // We need to toggle for the case where a Content-Length header is present.
                HttpChunk& next_chunk = res.chunks[res.next_chunk];
                if(res.not_chunked_transfer){
                    // If it is not a chunked transfer, then we assign Content-Length to
                    // the chunk size. And set the chunk_size_found, and chunk_body_start flags.
                    if(!next_chunk.chunk_size_found && !next_chunk.chunk_body_start){
                        auto it = std::find_if(res.headers.begin(), res.headers.end(), [](auto& header){
                            return header.field_name == HttpHeaderField::CONTENT_LENGTH;
                        });
                        next_chunk.chunk_size = HttpBigNum(HttpBigNum::dec, it->field_value);
                        next_chunk.chunk_size_started = true;
                        next_chunk.chunk_size_found = true;
                        next_chunk.chunk_body_start = true;
                    }
                    is >> next_chunk;
                    if(next_chunk.chunk_size == next_chunk.received_bytes){
                        // Finish parsing.
                        res.next_chunk = res.num_chunks;
                    }
                } else {
                    is >> next_chunk;
                    if(next_chunk.chunk_complete){
                        if(next_chunk.chunk_size != HttpBigNum{0}){
                            ++(res.num_chunks);
                            HttpChunk new_chunk = {
                                {0},"",{0},"",false,false,false,false,false
                            };
                            res.chunks.push_back(new_chunk);
                        }
                        ++(res.next_chunk);
                    }
                }
            }
            bytes_available = is.rdbuf()->in_avail();
        }
        return is;        
    }

}