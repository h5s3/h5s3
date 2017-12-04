#pragma once

#include <cstdint>

namespace h5s3::utils {
constexpr std::size_t operator""_KB(unsigned long long int n) {
    return n * 1024;
}

constexpr std::size_t operator""_MB(unsigned long long int n) {
    return n * 1024 * 1024;
}

constexpr std::size_t operator""_GB(unsigned long long int n) {
    return n * 1024 * 1024 * 1024;
}
}  // namespace h5s3::utils
