#ifndef UUID_HPP
#define UUID_HPP
#include <cstdint>
#include <iostream>
namespace UUID{
    struct Node {
        const static std::size_t length = 6;
        unsigned char bytes[length];
    };
    std::ostream& operator<<(std::ostream& os, const Node& node);
    std::istream& operator>>(std::istream& is, Node& node);

    // UUID binary fields as defined in IETF RFC 4122: 
    // https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.2
    // This is 16 octets of data. 
    // We set the default initialization of uuid structs to be all zeros.
    // Regardless of the UUID Version and Generation Scheme, the probability that 
    // a UUID will be all zeros should be extremely close to 0 (if not actually 0).
    // Note: The nature of UUIDs are that the probability that they are equal to any fixed
    // constant is extremely close to 0. Our choice of `0` is an arbitrary one.
    struct Uuid{
        constexpr static struct Version4{} v4{};
        constexpr static std::size_t size = 16; // UUID is always a 16 byte array.

        Uuid():bytes{}{}; // 0 initializing default constructor.
        Uuid(const Uuid& other); // copy constructor.
        explicit Uuid(Uuid::Version4 v); // explicit version 4 constructor.
        explicit Uuid(Uuid::Version4 v, const std::string& uuid); // Construct Uuid from string.

        // Public Member bytes.
        unsigned char bytes[Uuid::size];

        std::uint32_t time_low() const;
        std::uint16_t time_mid() const;
        std::uint16_t time_hi_and_version() const;
        unsigned char clock_seq_hi_and_reserved() const;
        unsigned char clock_seq_low() const;
        Node node() const;
    };
    std::ostream& operator<<(std::ostream& os, const Uuid& uuid);
    std::istream& operator>>(std::istream& is, Uuid& uuid);
    bool operator==(const Uuid& lhs, const Uuid& rhs);
    bool operator!=(const Uuid& lhs, const Uuid& rhs);

}// uuid namespace
#endif