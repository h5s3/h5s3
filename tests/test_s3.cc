#include <experimental/filesystem>

#include "gtest/gtest.h"

#include "h5s3/s3.h"
#include "process.h"

namespace fs = std::experimental::filesystem;
namespace s3 = h5s3::s3;

using process = h5s3::testing::process;

class S3Test : public ::testing::Test {
public:
    static std::string ACCESS_KEY;
    static std::string SECRET_KEY;
    static std::unique_ptr<process> minio_proc;

protected:

    static void SetUpTestCase() {
        auto path = fs::temp_directory_path();
        std::vector<std::string> args = {
            "testbin/minio",
            "server",
            "--config-dir", "testbin/minio-config/",
            "--address", "localhost:9999",
            "/tmp/minio/",
        };
        process::environment env = {
            {"MINIO_ACCESS_KEY", ACCESS_KEY},
            {"MINIO_SECRET_KEY", SECRET_KEY},
            {"PATH", std::getenv("PATH")},
        };
        minio_proc = std::make_unique<process>(args, env);
        // HACK: Find a better way to determine that the minio server is up and
        // running.
        sleep(1);

        std::vector<std::string> mc_args = {
            "testbin/mc",
            "--config-folder", "testbin/mc-config/", "--json",
            "config",
            "host",
            "remove",
            "h5s3-testing",
        };
        process::environment empty_env = {{"PATH", std::getenv("PATH")}};
        process proc(mc_args, empty_env);
        auto result = proc.join();
        EXPECT_EQ(result.code, 0);

        mc_args = {
            "testbin/mc",
            "--config-folder", "testbin/mc-config/", "--json",
            "config",
            "host",
            "add",
            "h5s3-testing",
            "http://localhost:9999",
            ACCESS_KEY, SECRET_KEY,
        };
        process proc2(mc_args, empty_env);
        result = proc2.join();
        EXPECT_EQ(result.code, 0);

        mc_args = {
            "testbin/mc",
            "--config-folder", "testbin/mc-config/", "--json",
            "mb", "--ignore-existing", "h5s3-testing/some-bucket",
        };
        process proc3(mc_args, empty_env);
        result = proc3.join();
        EXPECT_EQ(result.code, 0);
    }

    static void TearDownTestCase() {
        minio_proc->terminate();
        minio_proc->join();
        minio_proc.release();
    }
};

std::string S3Test::ACCESS_KEY("TESTINGACCESSKEYXXXX");
std::string S3Test::SECRET_KEY("TESTINGSECRETKEYXXXXXXXXXXXXXXXXXXXXXXXX");
std::unique_ptr<process> S3Test::minio_proc = nullptr;


TEST_F(S3Test, test_set_get) {
    s3::notary signer("us-east-1", ACCESS_KEY, SECRET_KEY);
    s3::set_object(signer, "some-bucket", "foo/bar", "my-content", "localhost:9999", false);
    std::string result = s3::get_object(signer, "some-bucket", "foo/bar", "localhost:9999", false);
    EXPECT_EQ(result, "my-content");
}
