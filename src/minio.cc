#include <algorithm>
#include <cerrno>

#include "h5s3/minio.h"

namespace h5s3::utils {

fs::path minio::temp_directory() {
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

    fs::path out;
    std::error_code ec;
    while (!fs::create_directory(out = generate_candidate(), ec)) {
        // The directory couldn't be created; this either means it already
        // exists or an error occurred.
        if (ec) {
            // An error occurred, throw the exception.
            throw std::runtime_error(ec.message());
        }
        // No error occurred, this means the directory already exists,
        // try again.
    }

    return out;
}

minio::minio(const std::string_view& access_key,
             const std::string_view& secret_key,
             const std::string_view& bucket,
             const std::string_view& address,
             const std::string_view& region)
    : m_access_key(access_key),
      m_secret_key(secret_key),
      m_test_bucket(bucket),
      m_minio_address(address),
      m_region(region),
      m_minio_root(temp_directory()) {
    std::string minio_config = m_minio_root / "minio-config/";

    // Start minio server.
    std::vector<std::string> args = {
        "testbin/minio",
        "server",
        "--quiet",
        "--config-dir",
        minio_config,
        "--address",
        m_minio_address,
        m_minio_root / "minio-data",
    };
    process::environment env = {
        {"MINIO_ACCESS_KEY", m_access_key},
        {"MINIO_SECRET_KEY", m_secret_key},
        {"MINIO_REGION", m_region},
        {"PATH", std::getenv("PATH")},
    };
    m_minio_proc = std::make_unique<process>(args, env);
    sleep(1);  // Give it a second to come up.

    // Create a bucket with mc.
    std::string mc_config = m_minio_root / "mc-config";
    auto mc_command = [&mc_config](const std::vector<std::string>& extra) {
        std::vector<std::string> argv = {
            "testbin/mc",
            "--quiet",
            "--config-folder",
            mc_config,
        };
        argv.insert(argv.end(), extra.begin(), extra.end());
        process::environment env = {{"PATH", std::getenv("PATH")}};
        process proc(argv, env);
        auto result = proc.join();
        if (result.code != 0) {
            throw std::runtime_error("failed to run mc");
        }
    };

    mc_command({"config", "host", "remove", "h5s3-testing"});
    mc_command({"config",
                "host",
                "add",
                "h5s3-testing",
                "http://" + m_minio_address,
                m_access_key,
                m_secret_key});
    mc_command({"mb",
                "--ignore-existing",
                "--region",
                m_region,
                "h5s3-testing/" + m_test_bucket});
}

minio::~minio() {
    m_minio_proc->terminate();
    m_minio_proc->join();
    fs::remove_all(m_minio_root);
}
}  // namespace h5s3::utils
