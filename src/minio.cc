#include <algorithm>
#include <cerrno>

#include "h5s3/minio.h"

namespace h5s3::utils {
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
      m_minio_root(tmpdir()) {
    std::string minio_config = m_minio_root.path() / "minio-config/";

    // Start minio server.
    std::vector<std::string> args = {
        "testbin/minio",
        "server",
        "--quiet",
        "--config-dir",
        minio_config,
        "--address",
        m_minio_address,
        m_minio_root.path() / "minio-data",
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
    std::string mc_config = m_minio_root.path() / "mc-config";
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
}
}  // namespace h5s3::utils
