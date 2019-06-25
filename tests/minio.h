#pragma once

#include <algorithm>
#include <cerrno>

#include "h5s3/private/utils.h"

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
    fs::path m_minio_root;

    /** Create a new, unique, temporary directory.
     */
    static fs::path temp_directory() {
        using namespace h5s3::utils;

        auto random_char = []() {
            auto options = "abcdefghijklmnopqrstuvwxyz0123456789"_array;
            return options[std::rand() % options.size()];
        };

        // generate a possible temporary directory path
        auto generate_candidate = [&]() {
            auto leaf = "h5s3-xxxxxx"_array;
            std::generate(std::next(leaf.begin(), 5), leaf.end(), random_char);
            return fs::temp_directory_path() / std::string(leaf.data(),
                                                           leaf.size());
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

public:

    minio()
        : m_access_key("TESTINGACCESSKEYXXXX"),
          m_secret_key("TESTINGSECRETKEYXXXXXXXXXXXXXXXXXXXXXXXX"),
          m_test_bucket("test-bucket"),
          m_minio_address("localhost:9999"),
          m_region("us-west-2"),
          m_minio_root(temp_directory()) {
        std::string minio_config = m_minio_root / "minio-config/";

        // Start minio server.
        std::vector<std::string> args = {
            "testbin/minio",
            "server",
            "--quiet",
            "--config-dir", minio_config,
            "--address", m_minio_address,
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
        auto mc_command = [&mc_config](const std::vector<std::string>& extra){
            std::vector<std::string> argv = {
                "testbin/mc",
                "--quiet",
                "--config-dir", mc_config,
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
        fs::remove_all(m_minio_root);
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
