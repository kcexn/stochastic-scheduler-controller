#ifndef UUID_TESTS_HPP
#define UUID_TESTS_HPP
#include "../../src/uuid/uuid.hpp"
namespace tests{
    class Uuid
    {
    public:
        constexpr static struct Version4{} v4{};
        Uuid(); // Default tests.
        explicit Uuid(Uuid::Version4); // Version 4 tests.

        operator bool(){ return passed_; }
    private:
        bool passed_;
        UUID::Uuid uuid_;
    };
}
#endif