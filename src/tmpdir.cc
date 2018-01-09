#include <algorithm>

#include "h5s3/private/tmpdir.h"
#include "h5s3/private/literals.h"

namespace h5s3::utils {
tmpdir::tmpdir() {
    auto random_char = []() {
        auto options = "abcdefghijklmnopqrstuvwxyz0123456789"_array;
        return options[std::rand() % options.size()];
    };

    // generate a possible temporary directory path
    auto generate_candidate = [&]() {
        auto leaf = "h5s3-xxxxxx"_array;
        std::generate(std::next(leaf.begin(), 5), leaf.end(), random_char);
        return fs::temp_directory_path() / std::string(leaf.data(), leaf.size());
    };

    std::error_code ec;
    while (!fs::create_directory(m_path = generate_candidate(), ec)) {
        // The directory couldn't be created; this either means it already
        // exists or an error occurred.
        if (ec) {
            // An error occurred, throw the exception.
            throw std::runtime_error(ec.message());
        }
        // No error occurred, this means the directory already exists,
        // try again.
    }
}

tmpdir::~tmpdir() {
    fs::remove_all(m_path);
}
}  // namespace h5s3::utils
