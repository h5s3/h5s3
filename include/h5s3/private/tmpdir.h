#pragma once

#include <experimental/filesystem>

namespace h5s3::utils {
namespace fs = std::experimental::filesystem;

/** A new, unique temporary directory.
 */
class tmpdir final {
private:
    fs::path m_path;
public:
    tmpdir();

    ~tmpdir();

    /** @return The path to this temporary directory.
     */
    inline const fs::path& path() const {
        return m_path;
    }
};
}  // namespace h5s3::utils
