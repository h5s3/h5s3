#pragma once

#include <algorithm>
#include <cerrno>

#include "h5s3/private/utils.h"

#include "h5s3/private/process.h"

namespace h5s3::utils {
namespace fs = std::experimental::filesystem;

class minio final {
private:
    std::string m_access_key;
    std::string m_secret_key;
    std::string m_test_bucket;
    std::string m_minio_address;
    std::string m_region;
    std::unique_ptr<process> m_minio_proc;
    fs::path m_minio_root;

    /** Create a new, unique, temporary directory.
     */
    static fs::path temp_directory();

public:
    minio(const std::string_view& access_key = "TESTINGACCESSKEYXXXX",
          const std::string_view& secret_key = "TESTINGSECRETKEYXXXXXXXXXXXXXXXXXXXXXXXX",
          const std::string_view& bucket = "test-bucket",
          const std::string_view& address = "localhost:9999",
          const std::string_view& region = "us-west-2");

    ~minio();

    inline const std::string& region() const {
        return m_region;
    }

    inline const std::string& access_key() const {
        return m_access_key;
    }

    inline const std::string& secret_key() const {
        return m_secret_key;
    }

    inline const std::string& bucket() const {
        return m_test_bucket;
    }

    inline const std::string& address() const {
        return m_minio_address;
    }
};
}  // namespace h5s3::utils
