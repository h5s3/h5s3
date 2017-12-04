#pragma once

#include <cstdint>

namespace h5s3::utils {

/** A pair of pointer and size.
 */
class out_buffer {
private:
    char* m_data;
    std::size_t m_size;

public:
    inline out_buffer(char* data, std::size_t size)
        : m_data(data), m_size(size) {}

    inline char* data() {
        return m_data;
    }

    inline char* data() const {
        return m_data;
    }

    inline std::size_t size() {
        return m_size;
    }

    inline std::size_t size() const {
        return m_size;
    }

    /** Slice the output buffer.

        @param offset The starting offset.
        @param size The length of the slice.
        @return A new output buffer which views a slice `this`.
    */
    inline out_buffer substr(std::size_t offset, std::size_t size) const {
        return {m_data + offset, size};
    }
};
}  // namespace h5s3::utils
