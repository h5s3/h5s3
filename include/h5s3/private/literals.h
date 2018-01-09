#pragma once

#include <array>
#include <cstdint>

namespace h5s3::utils {

/** User defined literal for kilobytes.
 */
constexpr std::size_t operator""_KB(unsigned long long int n) {
    return n * 1024;
}

/** User defined literal for megabytes.
 */
constexpr std::size_t operator""_MB(unsigned long long int n) {
    return n * 1024 * 1024;
}

/** User defined literal for gigabytes.
 */
constexpr std::size_t operator""_GB(unsigned long long int n) {
    return n * 1024 * 1024 * 1024;
}

template<typename Char, Char... cs>
constexpr std::array<Char, sizeof...(cs)> operator ""_array() {
    return { cs... };
}
}  // namespace h5s3::utils
