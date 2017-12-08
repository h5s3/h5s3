#pragma once

#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <sys/stat.h>

#include "h5s3/driver.h"
#include "h5s3/private/page.h"

namespace h5s3::file_driver {
namespace fs = std::experimental::filesystem;

/** A key-value store backed by a directory of files on the local file system.
 */
class file_kv_store final {
private:
    std::string m_path;
    driver::metadata m_metadata;

    inline file_kv_store(const std::string_view& path, std::size_t page_size)
        : m_path(path) {
        if (mkdir(m_path.data(), 0777) < 0 && errno != EEXIST) {
            throw std::runtime_error("failed to create directory");
        }

        std::fstream metafile(fs::path(m_path) / ".meta");
        if (metafile) {
            metafile >> m_metadata;
        }
        else {
            m_metadata.page_size = page_size;
            m_metadata.eof = 0;
        }
    }
public:
    inline file_kv_store(file_kv_store&& mvfrom) noexcept
        : m_path(std::move(mvfrom.m_path)),
          m_metadata(std::move(mvfrom.m_metadata)) {}

    static const char* name;

    inline static file_kv_store from_params(const std::string_view& path,
                                            unsigned int,
                                            std::size_t page_size) {
        return {path, page_size};
    }

    inline driver::metadata metadata() const {
        return m_metadata;
    }

    inline driver::metadata metadata() {
        return const_cast<const file_kv_store*>(this)->metadata();
    }

    std::unique_ptr<char[]> read(page::id page_id) const;
    void write(page::id page_id, const std::string_view& data);
    void flush();
    void truncate();
};

using file_driver = driver::kv_driver<file_kv_store>;
}  // namespace h5s3::file_driver
