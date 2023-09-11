#include "http-requests.hpp"
#include <charconv>
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace http
{
    HttpBigNum::HttpBigNum(const std::string& hex_str){
        const std::size_t max_str_width = 2*sizeof(std::size_t);
        std::vector<std::string> hex_str_partitions((hex_str.size()/max_str_width) + 1);
        std::size_t offset = 0;
        for(offset = 0; offset < hex_str_partitions.size()-1; ++offset){
            hex_str_partitions[offset].append(hex_str.end() - (offset+1)*max_str_width, hex_str.end()-(offset)*max_str_width);
        }
        hex_str_partitions[offset].append(hex_str.begin(), hex_str.end() - (offset)*max_str_width);
        for(auto it=hex_str_partitions.rbegin(); it != hex_str_partitions.rend(); ++it){
            std::size_t num{};
            std::from_chars_result res = std::from_chars(it->data(), it->data()+it->size(), num, 16);
            if(res.ec != std::errc{}){
                throw "char conversion failed.";
            }
            push_back(num);
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
        if( it == rend()){
            // remove leading zeroes.
            while(front() == 0){
                erase(begin());
            }
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
        // remove all of the leading 0s.
        while(front() == 0){
            erase(begin());
        }
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
        // remove all of the leading 0s.
        while(front() == 0){
            erase(begin());
        }
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

    std::ostream& operator<<(std::ostream& os, HttpBigNum& rhs){
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
        std::locale loc = is.getloc();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        // While there are bytes available in the input stream, keep parsing.
        while(bytes_available > 0 && !chunk.chunk_complete){
            if(!chunk.chunk_body_start){
                if(!chunk.chunk_size_found){
                    // This is the initial state for parsing a new chunk.
                    // Read until we have skipped all of the leading whitespaces
                    // according to the current locale.
                    char first_char;
                    while(bytes_available > 0){
                        is.get(first_char);
                        --bytes_available;
                        if(!std::isspace(first_char,loc)){
                            break;
                        }
                    }
                    // first char is now the first non-whitespace character.
                    chunk.chunk_header.push_back(first_char);

                    // Now we are scanning the chunk-size portion of the string.
                    // Push back all of the next non-white space characters.
                    char last_char;
                    while(bytes_available > 0){
                        is.get(last_char);
                        --bytes_available;
                        if(!std::isspace(last_char,loc)){
                            chunk.chunk_header.push_back(last_char);
                        } else {
                            //last_char now points to the first white space character after the
                            //chunk-size.
                            chunk.chunk_size_found = true;
                            break;
                        }
                    }
                    if(chunk.chunk_size_found){
                        // Set the chunk size.
                        chunk.chunk_size = HttpBigNum(chunk.chunk_header);
                        // free the memory in the chunk header.
                        chunk.chunk_header.clear();
                        chunk.chunk_header.shrink_to_fit();
                    }
                }
                // The chunk size has been found at this point, now we just need to seek the stream until we find a new line.
                char cur;
                while(bytes_available > 0){
                    is.get(cur);
                    --bytes_available;
                    if(cur == '\n'){
                        chunk.chunk_body_start = true;
                        break;
                    }
                }
            } else if(chunk.received_bytes < chunk.chunk_size){
                // Fill the buffer with chunk-size bytes.
                char cur;
                while(bytes_available > 0 && chunk.received_bytes < chunk.chunk_size){
                    is.get(cur);
                    --bytes_available;
                    chunk.chunk_data.push_back(cur);
                    ++(chunk.received_bytes);
                }
            } else {
                // There is a distinct possibility that there is still more data appended to the chunk past the end of chunk size.
                // In this particular case we will keep seeking the stream until we find a new line, and set the get pointer to that position.
                char cur;
                while(bytes_available > 0){
                    is.get(cur);
                    --bytes_available;
                    if(cur == '\n'){
                        chunk.chunk_complete = true;
                        break;
                    }
                }
            }
            bytes_available = is.rdbuf()->in_avail();
        }
        return is;
    }

    std::ostream& operator<<(std::ostream& os, HttpChunk& chunk){
        os << chunk.chunk_size << "\r\n"
           << chunk.chunk_data << "\r\n";
        return os;
    }

    std::istream& operator>>(std::istream& is, HttpHeader& header){
        // Get the stream locale for parsing.
        std::locale loc = is.getloc();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        // While there are bytes available in the input stream, keep parsing.
        while(bytes_available > 0 && !header.header_complete){
            if(!header.field_name_found){
                char cur;
                while(bytes_available > 0){
                    //Seek until either a non-white space character, or a new line.
                    if(!header.not_last){
                        is.get(cur);
                        --bytes_available;
                        if(cur == '\n'){
                            // A new line was found without finding any
                            // non-whitespace characters. Therefore this is the last header.
                            // Mark it as the last header, and break.
                            header.field_name_found = true;
                            header.field_name = HttpHeaderField::END_OF_HEADERS;
                            header.header_complete = true;
                            break;
                        } else if(!std::isspace(cur, loc)){
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

                         is.get(cur);
                        --bytes_available;                       
                        // Push all of the subsequent non white-space and non-delimter ':'
                        // characters onto the buffer.
                        if(!(std::isspace(cur,loc) || cur == ':')){
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
                            
                            // This means that the header field name has been found.
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
                            } else if (header.buf == "CONNECT") {
                                header.field_name = HttpHeaderField::CONNECT;
                            } else {
                                header.field_name = HttpHeaderField::UNKNOWN;
                            }
                            // Free the memory in the header buffer.
                            header.buf.clear();
                            header.buf.shrink_to_fit();
                            break;
                        }
                    }
                }
            } else {
                // header field name has been found.
                char c;
                while(bytes_available > 0){
                    // First, if the header field delimiter ':' has not been found yet, we need to seek for it.
                    is.get(c);
                    --bytes_available;
                    if (!header.field_delimiter_found && c == ':'){
                        // We have found the delimiter.
                        header.field_delimiter_found = true;
                    } else {
                        // Seek for the first non-white space character.
                        // OR a new line character, which marks the end of the header field value (i.e.; empty header field).
                        if(c == '\n'){
                            // The newline character marks the end of the header field value.
                            // Technically, there is an exception for the message/http media type that delimited line folding is allowed.
                            // However for the purposes of my application, I am only planning on implementing the application/json media type.
                            header.header_complete = true;
                            break;
                        } else if(!std::isspace(c,loc) && !header.field_value_started){
                            // The first non-whitespace character marks the beginning of the field falues.
                            header.field_value_started = true;
                            header.field_value.push_back(c);
                        } else if(header.field_value_started && !header.field_value_ended){
                            // The first non-white space character has been found,
                            // that means that all visible ASCII character + spaces + tabs 
                            // are part of the header field value, otherwise, the field 
                            // value has finished.
                            if(!std::isspace(c,loc) || c == ' ' || c == '\t'){
                                header.field_value.push_back(c);
                            } else {
                                header.field_value_ended = true;
                            }
                        }
                    }
                }
            }
            bytes_available = is.rdbuf()->in_avail();
        }
        return is;
    }

    std::ostream& operator<<(std::ostream& os, HttpHeader& header){
        switch(header.field_name)
        {
            case HttpHeaderField::CONTENT_TYPE:
                os << "Content-Type: ";
            case HttpHeaderField::CONTENT_LENGTH:
                os << "Content-Length: ";
            case HttpHeaderField::ACCEPT:
                os << "Accept: ";
            case HttpHeaderField::HOST:
                os << "Host: ";
            case HttpHeaderField::TRANSFER_ENCODING:
                os << "Transfer-Encoding: ";
            case HttpHeaderField::CONNECT:
                os << "Connect: ";
            default:
                return os;
        }
        os << header.field_value << "\r\n";
        return os;
    }

}