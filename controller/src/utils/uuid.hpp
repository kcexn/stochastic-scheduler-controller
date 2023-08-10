#ifndef UUID_HPP
#define UUID_HPP

#include <cstdint>

namespace uuid{

    // UUID binary fields as defined in IETF RFC 4122: 
    // https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.2
    // This is 16 octets of data. 
    // We set the default initialization of uuid structs to be all zeros.
    // Regardless of the UUID Version and Generation Scheme, the probability that 
    // a UUID will be all zeros should be extremely close to 0 (if not actually 0).
    // Note: The nature of UUIDs are that the probability that they are equal to any fixed
    // constant is extremely close to 0. Our choice of `0` is an arbitrary one.
    typedef struct
    {
        uint32_t time_low = 0; // 4 octets
        uint16_t time_mid = 0; // 2 octets
        uint16_t time_hi_and_version = 0; // 2 octets
        char clock_seq_hi_and_reserved = 0; // 1 octet
        char clock_seq_low = 0; // 1 octet
        char node[6] = {0,0,0,0,0,0}; // 6 octets
    } uuid;

    // bit masks for UUID Versions
    enum{
        UUID_V1 = 0x0100,
        #define UUID_V1 UUID_V1
        UUID_V2 = 0x0200,
        #define UUID_V2 UUID_V2
        UUID_V3 = 0x0300,
        #define UUID_V3 UUID_V3
        UUID_V4 = 0x0400,
        #define UUID_V4 UUID_V4
        UUID_V5 = 0x0500,
        #define UUID_V5 UUID_V5
    };

}// uuid namespace

#endif