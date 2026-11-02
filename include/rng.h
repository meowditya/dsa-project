// include/rng.h — the only source of randomness in the project. Owner: Architect.
//
// Why not std::uniform_int_distribution & co.? The C++ standard fixes the exact output of the
// std::mt19937_64 engine, but it does not fix the algorithms of the std::*_distribution classes,
// so they can give different numbers on different standard libraries. Mapping raw 64-bit draws
// to numbers ourselves keeps every run reproducible.
#pragma once
#include <cstdint>
#include <random>
#include <stdexcept>

namespace eq {

struct Rng {
    std::mt19937_64 g;
    explicit Rng(std::uint64_t seed) : g(seed) {}

    // Uniform integer in [0, n), exactly unbiased, by rejection sampling.
    // threshold = 2^64 mod n (the uint64 subtraction wraps on purpose). Draws below it are
    // rejected, so the accepted range [threshold, 2^64) has a size divisible by n and r % n
    // is uniform. For n = 10^6 a draw is rejected with probability ~3e-14.
    // Always consumes at least one engine draw, even for n == 1 (keeps runs aligned).
    std::uint64_t below(std::uint64_t n) {
        if (n == 0) throw std::invalid_argument("Rng::below(0)");
        const std::uint64_t threshold = (std::uint64_t{0} - n) % n;
        std::uint64_t r;
        do { r = g(); } while (r < threshold);
        return r % n;
    }

    // Uniform double in [0, 1) from the top 53 bits of one draw. Used only by benchmark input
    // generation (distributions); the simulator uses integers only.
    double u01() { return static_cast<double>(g() >> 11) * (1.0 / 9007199254740992.0); }
};

}  // namespace eq
