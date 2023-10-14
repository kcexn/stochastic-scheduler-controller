#ifndef CONTROLLER_RESOURCES_HPP
#define CONTROLLER_RESOURCES_HPP
#include "run/run.hpp"
#include "init/init.hpp"
/* First 40 prime numbers */
const std::size_t PRIMES[100] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 71, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173
};
#define PRIME_GENERATOR(N) (PRIMES[(N/3+2)])
#endif