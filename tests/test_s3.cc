#include <experimental/filesystem>

#include "gtest/gtest.h"

#include "h5s3/minio.h"
#include "h5s3/private/curl.h"
#include "h5s3/s3.h"

namespace s3 = h5s3::s3;

class S3Test : public ::testing::Test {
protected:
    static std::unique_ptr<h5s3::utils::minio> MINIO;

    s3::notary notary;

    S3Test() : notary(MINIO->region(), MINIO->access_key(), MINIO->secret_key()) {}

    static void SetUpTestCase() {
        MINIO = std::make_unique<h5s3::utils::minio>();
    }

    static void TearDownTestCase() {
        MINIO.reset();
    }
};

std::unique_ptr<h5s3::utils::minio> S3Test::MINIO = nullptr;

TEST_F(S3Test, set_then_get) {
    auto key = "some/key";
    for (auto content : {"content1", "content2", "content3"}) {
        s3::set_object(notary, MINIO->bucket(), key, content, MINIO->address(), false);
        std::string result =
            s3::get_object(notary, MINIO->bucket(), key, MINIO->address(), false);
        EXPECT_EQ(result, content);
    }
}

TEST_F(S3Test, get_into) {
    std::array<char, 1024> content;
    content.fill('X');

    auto key = "get_into";
    s3::set_object(notary,
                   MINIO->bucket(),
                   key,
                   {content.data(), content.size()},
                   MINIO->address(),
                   false);

    std::array<char, content.size()> outbuf_memory = {0};
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());

    std::size_t bytes_read =
        s3::get_object(outbuf, notary, MINIO->bucket(), key, MINIO->address(), false);
    ASSERT_EQ(bytes_read, content.size());
    ASSERT_EQ(outbuf_memory, content);
}

TEST_F(S3Test, get_less_than_full) {
    std::array<char, 512> content;
    content.fill('X');

    auto key = "get_less_than_full";
    s3::set_object(notary,
                   MINIO->bucket(),
                   key,
                   {content.data(), content.size()},
                   MINIO->address(),
                   false);

    // Twice as much output memory as the content size.
    std::array<char, content.size() * 2> outbuf_memory;
    outbuf_memory.fill('Z');
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());

    std::size_t bytes_read =
        s3::get_object(outbuf, notary, MINIO->bucket(), key, MINIO->address(), false);

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
                   MINIO->bucket(),
                   key,
                   {content.data(), content.size()},
                   MINIO->address(),
                   false);

    // Outbuf not large enough to hold content.
    std::array<char, content.size() - 1> outbuf_memory;
    h5s3::utils::out_buffer outbuf(outbuf_memory.data(), outbuf_memory.size());
    ASSERT_THROW(
        {
            s3::get_object(outbuf, notary, MINIO->bucket(), key, MINIO->address(), false);
        },
        std::out_of_range);
}

TEST_F(S3Test, get_nonexistent) {
    auto key = "some/non/existent/key";
    EXPECT_THROW(
        { s3::get_object(notary, MINIO->bucket(), key, MINIO->address(), false); },
        h5s3::curl::http_error);
}
