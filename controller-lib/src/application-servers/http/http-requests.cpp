#include "http-requests.hpp"
#include <charconv>
namespace http
{
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