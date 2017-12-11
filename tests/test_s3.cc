#include <experimental/filesystem>

#include "gtest/gtest.h"

#include "h5s3/s3.h"
#include "h5s3/private/curl.h"
#include "process.h"

namespace fs = std::experimental::filesystem;
namespace s3 = h5s3::s3;

using process = h5s3::testing::process;

class S3Test : public ::testing::Test {
public:
    static std::string ACCESS_KEY;
    static std::string SECRET_KEY;
    static std::string TEST_BUCKET;
    static std::string MINIO_ADDRESS;
    static std::string REGION;
    static std::unique_ptr<process> minio_proc;

    s3::notary notary;

protected:

    S3Test() : notary(REGION, ACCESS_KEY, SECRET_KEY) {};

    static void SetUpTestCase() {
        auto path = fs::temp_directory_path();
        std::string minio_config = path / "minio-config/";

        // Start minio server.
        std::vector<std::string> args = {
            "testbin/minio",
            "server",
            "--quiet",
            "--config-dir", minio_config,
            "--address", MINIO_ADDRESS,
            path / "minio-data",
        };
        process::environment env = {
            {"MINIO_ACCESS_KEY", ACCESS_KEY},
            {"MINIO_SECRET_KEY", SECRET_KEY},
            {"MINIO_REGION", REGION},
            {"PATH", std::getenv("PATH")},
        };
        minio_proc = std::make_unique<process>(args, env);
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
            EXPECT_EQ(result.code, 0);
        };
        mc_command({"config", "host", "remove", "h5s3-testing"});
        mc_command({"config", "host", "add", "h5s3-testing",
                    "http://" + MINIO_ADDRESS, ACCESS_KEY, SECRET_KEY});
        mc_command({"mb", "--ignore-existing",
                    "--region", REGION,
                    "h5s3-testing/" + TEST_BUCKET});
    }

    static void TearDownTestCase() {
        minio_proc->terminate();
        minio_proc->join();
        minio_proc.reset();
    }
};

std::string S3Test::ACCESS_KEY("TESTINGACCESSKEYXXXX");
std::string S3Test::SECRET_KEY("TESTINGSECRETKEYXXXXXXXXXXXXXXXXXXXXXXXX");
std::string S3Test::TEST_BUCKET("test-bucket");
std::string S3Test::MINIO_ADDRESS("localhost:9999");
std::string S3Test::REGION("us-west-2");
std::unique_ptr<process> S3Test::minio_proc = nullptr;

TEST_F(S3Test, set_then_get) {
    auto key = "some/key";
    for (auto content : {"content1", "content2", "content3"}) {
        s3::set_object(notary, TEST_BUCKET, key, content, MINIO_ADDRESS, false);
        std::string result = s3::get_object(notary, TEST_BUCKET, key, MINIO_ADDRESS, false);
        EXPECT_EQ(result, content);
    }
}

TEST_F(S3Test, get_into) {
    std::array<char, 1024> content;
    content.fill('X');

    auto key = "get_into";
    s3::set_object(notary,
                   TEST_BUCKET,
                   key,
                   {content.data(), content.size()},
                   MINIO_ADDRESS,
                   false);

    std::array<char, content.size()> outbuf_memory = {0};
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());

    std::size_t bytes_read = s3::get_object(outbuf,
                                            notary,
                                            TEST_BUCKET,
                                            key,
                                            MINIO_ADDRESS,
                                            false);
    ASSERT_EQ(bytes_read, content.size());
    ASSERT_EQ(outbuf_memory, content);
}

TEST_F(S3Test, get_less_than_full) {
    std::array<char, 512> content;
    content.fill('X');

    auto key = "get_less_than_full";
    s3::set_object(notary,
                   TEST_BUCKET,
                   key,
                   {content.data(), content.size()},
                   MINIO_ADDRESS,
                   false);

    // Twice as much output memory as the content size.
    std::array<char, content.size() * 2> outbuf_memory;
    outbuf_memory.fill('Z');
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());

    std::size_t bytes_read = s3::get_object(outbuf,
                                            notary,
                                            TEST_BUCKET,
                                            key,
                                            MINIO_ADDRESS,
                                            false);

    // We should only get content_size bytes back.
    ASSERT_EQ(bytes_read, content.size());
    ASSERT_TRUE(bytes_read < outbuf.size());

    // Initial bytes of the outbuf should match the expected content.
    ASSERT_EQ(std::string_view(outbuf_memory.data(), content.size()),
              std::string_view(content.data(), content.size()));

    // Trailing bytes should hold their old value.
    std::array<char, outbuf_memory.size() - content.size()> Zs;
    Zs.fill('Z');
    ASSERT_TRUE(std::equal(Zs.begin(),
                           Zs.end(),
                           outbuf_memory.begin() + content.size(),
                           outbuf_memory.end()));
}

TEST_F(S3Test, get_more_than_full) {
    std::array<char, 512> content;
    content.fill('X');

    auto key = "get_more_than_full";
    s3::set_object(notary,
                   TEST_BUCKET,
                   key,
                   {content.data(), content.size()},
                   MINIO_ADDRESS,
                   false);

    // Outbuf not large enough to hold content.
    std::array<char, content.size() - 1> outbuf_memory;
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());
    ASSERT_THROW({s3::get_object(outbuf,
                                 notary,
                                 TEST_BUCKET,
                                 key,
                                 MINIO_ADDRESS,
                                 false); },
        std::out_of_range);
}

TEST_F(S3Test, get_nonexistent) {
    auto key = "some/non/existent/key";
    EXPECT_THROW({ s3::get_object(notary, TEST_BUCKET, key, MINIO_ADDRESS, false); }, h5s3::curl::http_error);
}
