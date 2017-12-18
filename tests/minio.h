#pragma once

#include "process.h"

namespace fs = std::experimental::filesystem;

using process = h5s3::testing::process;

class minio {
private:
    std::string m_access_key;
    std::string m_secret_key;
    std::string m_test_bucket;
    std::string m_minio_address;
    std::string m_region;
    std::unique_ptr<process> m_minio_proc;

public:

    minio()
        : m_access_key("TESTINGACCESSKEYXXXX"),
          m_secret_key("TESTINGSECRETKEYXXXXXXXXXXXXXXXXXXXXXXXX"),
          m_test_bucket("test-bucket"),
          m_minio_address("localhost:9999"),
          m_region("us-west-2") {
        auto path = fs::temp_directory_path();
        std::string minio_config = path / "minio-config/";

        // Start minio server.
        std::vector<std::string> args = {
            "testbin/minio",
            "server",
            "--quiet",
            "--config-dir", minio_config,
            "--address", m_minio_address,
            path / "minio-data",
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
        std::string mc_config = path / "mc-config";
        auto mc_command = [&mc_config](const std::vector<std::string>& extra){
            std::vector<std::string> argv = {
                "testbin/mc",
                "--quiet",
                "--config-folder", mc_config,
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
        mc_command({"config", "host", "add", "h5s3-testing",
                    "http://" + m_minio_address, m_access_key, m_secret_key});
        mc_command({"mb", "--ignore-existing",
                    "--region", m_region,
                    "h5s3-testing/" + m_test_bucket});
    }

    ~minio() {
        m_minio_proc->terminate();
        m_minio_proc->join();
    }

    const std::string& region() const {
        return m_region;
    }

    const std::string& access_key() const {
        return m_access_key;
    }

    const std::string& secret_key() const {
        return m_secret_key;
    }

    const std::string& bucket() const {
        return m_test_bucket;
    }

    const std::string& address() const {
        return m_minio_address;
    }
};
