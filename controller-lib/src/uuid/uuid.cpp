#include "uuid.hpp"
#include <cstring>
#include <iomanip>
#include <charconv>
#include <sys/random.h>

/*bit masks for UUID Versions.*/
#define UUID_VERSION_1 0x1000
#define UUID_VERSION_2 0x2000
#define UUID_VERSION_3 0x3000
#define UUID_VERSION_4 0x4000
#define UUID_VERSION_5 0x5000

namespace UUID{
    /*UUID.Node POD*/
    std::ostream& operator<<(std::ostream& os, const Node& node) {     
        std::size_t offset=2;  
        for(std::size_t i=0; i < (Node::length/offset); ++i){
            std::uint16_t tmp;
            std::memcpy(&tmp, &(node.bytes[i*offset]), offset);
            os << std::setfill('0') << std::setw(4) << std::hex << tmp;
        }
        return os;
    }

    std::istream& operator>>(std::istream& is, Node& node){
        char hex_str[4] = {};
        std::size_t offset=2;
        for(std::size_t i=0; i < (Node::length/offset); ++i){
            std::uint16_t tmp = 0;
            is.read(hex_str, 4);
            std::from_chars_result res = std::from_chars(hex_str, hex_str+4, tmp, 16);
            if(res.ec != std::errc{}){
                std::cerr << "node conversion failed." << std::endl;
            }
            std::memcpy(&(node.bytes[i*offset]), &tmp, offset);
        }
        return is;
    }

    /*UUID*/
    Uuid::Uuid(const Uuid& other)
    {
        std::memcpy(bytes, other.bytes, Uuid::size);
    }

    Uuid::Uuid(Uuid::Version4)
      : bytes{}
    {
        // This is a bit dodgy since it means that 
        // the constructor doesn't necessarily stop after a deterministic amount of time.
        // In practice though, this will almost alosay stop after 1 iteration, 
        // since 16 bytes of random data is not very much.
        std::size_t length = 0;
        do{
            length = getrandom(bytes, Uuid::size, 0);
        } while(length != Uuid::size);
        // UUID clock_seq_hi_and_reserved is byte 9
        unsigned char& clock_seq_hi_and_reserved = bytes[8];
        // Set the hi bit to 1.
        clock_seq_hi_and_reserved |= 0x80;
        // Mask the second highest bit to low.
        clock_seq_hi_and_reserved &= 0xbf;
        // Time hi and version are bytes 7 and 8.
        std::uint16_t time_hi_and_version = 0;
        std::memcpy(&time_hi_and_version, &bytes[6], 2);
        // Set the version 4 bit.
        time_hi_and_version |= UUID_VERSION_4;
        // Mask out the random noise.
        time_hi_and_version &= 0x4FFF;
        std::memcpy(&bytes[6], &time_hi_and_version, 2);
        return;
    }

    Uuid::Uuid(Uuid::Version4, const std::string& uuid)
      : bytes{}
    {
        Uuid tmp(Uuid::v4);
        std::stringstream ss(uuid);
        ss >> tmp;
        std::memcpy(bytes, tmp.bytes, Uuid::size);
    }

    std::uint32_t Uuid::time_low() const {
        std::uint32_t tmp = 0;
        std::memcpy(&tmp, &bytes[0], 4);
        return tmp;
    }
    std::uint16_t Uuid::time_mid() const {
        std::uint16_t tmp = 0;
        std::memcpy(&tmp, &bytes[4], 2);
        return tmp;
    }
    std::uint16_t Uuid::time_hi_and_version() const {
        std::uint16_t tmp =0;
        std::memcpy(&tmp, &bytes[6], 2);
        return tmp;
    }
    unsigned char Uuid::clock_seq_hi_and_reserved() const {
        unsigned char tmp = 0;
        std::memcpy(&tmp, &bytes[8], 1);
        return tmp;
    }
    unsigned char Uuid::clock_seq_low() const {
        unsigned char tmp = 0;
        std::memcpy(&tmp, &bytes[9], 1);
        return tmp;
    }
    Node Uuid::node() const {
        Node tmp = {};
        std::memcpy(tmp.bytes, &bytes[10], Node::length);
        return tmp;
    }

    std::ostream& operator<<(std::ostream& os, const Uuid& uuid){
        os << std::setfill('0') << std::setw(8) << std::hex << uuid.time_low();
        os << std::setfill('0') << std::setw(4) << std::hex << uuid.time_mid();
        os << std::setfill('0') << std::setw(4) << std::hex << uuid.time_hi_and_version();
        os << std::setfill('0') << std::setw(2) << std::hex << static_cast<std::uint16_t>(uuid.clock_seq_hi_and_reserved());
        os << std::setfill('0') << std::setw(2) << std::hex << static_cast<std::uint16_t>(uuid.clock_seq_low());
        os << uuid.node();
        return os;
    }
    
    std::istream& operator>>(std::istream& is, Uuid& uuid){
        char hex_str[8] = {};

        // extract time_low.
        std::size_t num_chars = 8;
        std::uint32_t time_low = 0;
        is.read(hex_str, num_chars);
        std::from_chars_result res = std::from_chars(hex_str, hex_str+num_chars, time_low, 16);
        if (res.ec != std::errc{}){
            std::cerr << "time low conversion failed." << std::endl;
        }
        std::memcpy(&(uuid.bytes[0]), &time_low, (num_chars>>1));

        // extract time_mid.
        num_chars = 4;
        std::uint16_t time_mid = 0;
        is.read(hex_str, num_chars);
        res = std::from_chars(hex_str, hex_str+num_chars, time_mid, 16);
        if (res.ec != std::errc{}){
            std::cerr << "time mid conversion failed." << std::endl;
        }
        std::memcpy(&(uuid.bytes[4]), &time_mid, (num_chars>>1));

        // Extract time hi and version.
        std::uint16_t time_hi_and_version = 0;
        is.read(hex_str, num_chars);
        res = std::from_chars(hex_str, hex_str+num_chars, time_hi_and_version, 16);
        if (res.ec != std::errc{}){
            std::cerr << "time hi and version conversion failed." << std::endl;
        }
        std::memcpy(&(uuid.bytes[6]), &time_hi_and_version, (num_chars>>1));

        // extract clock seq hi and reserved.
        num_chars = 2;
        unsigned char clock_seq_hi_and_reserved = 0;
        is.read(hex_str, num_chars);
        res = std::from_chars(hex_str, hex_str+num_chars, clock_seq_hi_and_reserved, 16);
        if (res.ec != std::errc{}){
            std::cerr << "clock seq hi and reserved conversion failed." << std::endl;
        }
        std::memcpy(&(uuid.bytes[8]), &clock_seq_hi_and_reserved, (num_chars>>1));

        // extract clock seq low.
        unsigned char clock_seq_low = 0;
        is.read(hex_str, num_chars);
        res = std::from_chars(hex_str, hex_str+num_chars, clock_seq_low, 16);
        if(res.ec != std::errc{}){
            std::cerr << "clock seq low conversion failed." << std::endl;
        }
        std::memcpy(&(uuid.bytes[9]), &clock_seq_low, (num_chars>>1));

        //extract node.
        Node node = {};
        is >> node;
        std::memcpy(&(uuid.bytes[10]), node.bytes, 6);
        
        return is;
    }


    bool operator==(const Uuid& lhs, const Uuid& rhs){
        for(std::size_t i=0; i < Uuid::size; ++i){
            if(lhs.bytes[i] != rhs.bytes[i]){
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Uuid&lhs, const Uuid& rhs){
        return !(lhs == rhs);
    }
}