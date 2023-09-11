#include "http-requests.hpp"
#include <charconv>
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
        bool carry{false};
        std::size_t i = size();
        do{
             // num[i] is as large as it can go.
            if((*this)[i-1] == std::numeric_limits<std::size_t>::max()){
                // wrap num[i] back around to 0.
                (*this)[i-1] = 0;
                // Set the carry flag.
                carry = true;
            } else {
                ++((*this)[i-1]);
                carry = false;
            }
            --i;  
        } while(i > 0 && carry);
        // if i==0 that means that the big number has overflowed.
        // the number should be extended, and then 1 added to the most significant number.
        if(i==0){
            auto it = begin();
            insert(it, 1);
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
            // if subtraction overflows we set the number
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

    std::istream& operator>>(std::istream& is, HttpChunk& chunk){
        // Store the start position of the current stream extraction attempt.
        std::istream::pos_type start=is.tellg();
        std::streamsize bytes_available = is.rdbuf()->in_avail();
        // Get the stream locale for parsing.
        std::locale loc = is.getloc();

        // While there are bytes available in the input stream, keep parsing.
        while(bytes_available > 0){
            if(!chunk.chunk_size_found){
                // This is the initial state for parsing a new chunk.
                // Read until we have skipped all of the leading whitespaces
                // according to the current locale.
                char first_char = '\0';
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
                char last_char = '\0';
                while(bytes_available > 0){
                    is.get(last_char);
                    --bytes_available;
                    if(!std::isspace(last_char,loc)){
                        chunk.chunk_header.push_back(last_char);
                    } else {
                        break;
                    }
                }
                //last_char now points to the first white space character after the
                //chunk-size.

                // The chunk_header string now refers to the chunk-size hex string.
                // Convert the chunk-size hex string to a size_t iff the hex string isn't too wide.
                if(chunk.chunk_header.size() > HttpChunk::max_hex_str_width){
                    throw "Invalid Chunk Size.";
                }
                
                // Convert the chunk-size from a hex string to a size_t.
                std::size_t chunk_size = 0;
                std::from_chars_result res = std::from_chars(chunk.chunk_header.data(), chunk.chunk_header.data()+chunk.chunk_header.size(), chunk_size, 16);
                if(res.ec != std::errc{}){
                    throw "string conversion failed.";
                }

                // Set the chunk size, set the chunk size found flag, and update the bytes_available parameter.
                chunk.chunk_size = chunk_size;
                chunk.chunk_size_found = true;

                // Seek through the stream until we find a new line.
                char cur = '\0';
                while(bytes_available > 0){
                    is.get(cur);
                    --bytes_available;
                    if(cur == '\n'){
                        break;
                    }
                }
            } else if(chunk.chunk_data.size() != chunk.chunk_size){
                // Fill the buffer with chunk-size bytes.
                char curr = '\0';
                while(bytes_available > 0 && chunk.chunk_data.size() < chunk.chunk_size){
                    is.get(curr);
                    --bytes_available;
                    chunk.chunk_data.push_back(curr);
                }
                if(chunk.chunk_data.size() == chunk.chunk_size){
                    chunk.chunk_complete=true;
                }
            } else {
                // If there are chunk_size bytes in chunk_data, then we are done parsing this chunk.
                chunk.chunk_complete = true;

                // There is a distinct possibility that there is still more data appended to the chunk past the end of chunk size.
                // In this particular case we will keep seeking the stream until we find a new line, and set the get pointer to that position.
                char cur = '\0';
                while(bytes_available > 0){
                    is.get(cur);
                    --bytes_available;
                    if(cur == '\n'){
                        break;
                    }
                }
            }
        }
        return is;
    }
}