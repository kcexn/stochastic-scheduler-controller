#include "uuid.hpp"
#include <cstring>
#include <sys/random.h>
#include <iomanip>

namespace UUID{
    std::ostream& operator<<(std::ostream& out, uuid_t& rhs)
    {
        std::ios::fmtflags out_flags(out.flags());
        // io stream overloading for uuids using cpp string streams.
        out.setf(std::ios::hex, std::ios::basefield);

        out << "0x" << rhs.time_low << "." 
            << rhs.time_mid << "." 
            << rhs.time_hi_and_version << "."
            << (static_cast<int>(rhs.clock_seq_hi_and_reserved)&0xFF) << "."
            << (static_cast<int>(rhs.clock_seq_low)&0xFF) << ".";
        for (int i=0; i < 6; ++i){
            out << (static_cast<int>(rhs.node[i])&0xFF);
        }
        out.flags(out_flags);
        return out;
    }

    bool operator==(const uuid_t& lhs, const uuid_t& rhs){
        return (
            lhs.time_low == rhs.time_low &&
            lhs.time_mid == rhs.time_mid &&
            lhs.time_hi_and_version == rhs.time_hi_and_version &&
            lhs.clock_seq_hi_and_reserved == rhs.clock_seq_hi_and_reserved &&
            lhs.clock_seq_low == rhs.clock_seq_low &&
            lhs.node[0] == rhs.node[0] &&
            lhs.node[1] == rhs.node[2] &&
            lhs.node[3] == rhs.node[3] &&
            lhs.node[4] == rhs.node[4] &&
            lhs.node[5] == rhs.node[5]
        );
    }

    uuid_t uuid_create_v4()
    {
        // Generates a random v4 uuid.
        UUID::uuid_t uuid_v4;

        char buf[16] = {};
        std::size_t buflen = 16;
        std::size_t length = getrandom(buf, buflen, 0);
        if (length != 16){
            // Return a default constructed uuid (all zeroes, if the getrandom write failed.)
            UUID::uuid_t uuid_error = {};
            return uuid_error;
        }
        // getrandom has filled the entire buffer.
        std::memcpy(&uuid_v4, buf, buflen);
        // Set the hi bit to 1.
        uuid_v4.clock_seq_hi_and_reserved |= 0x80;
        // Mask the second highest bit to low.
        uuid_v4.clock_seq_hi_and_reserved &= 0xbf;
        // Set the version 4 bits.
        uuid_v4.time_hi_and_version |= UUID_V4;
        // Mask out the random noise.
        uuid_v4.time_hi_and_version &= 0x4FFF;
        return uuid_v4;
    }
}