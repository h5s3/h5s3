#include "gtest/gtest.h"

#include "h5s3/private/hash.h"

template<typename Char, Char... cs>
constexpr std::array<unsigned char, sizeof...(cs)> operator ""_arr() {
    return { static_cast<unsigned char>(cs)... };
}

TEST(hash, sha256) {
    auto result = h5s3::hash::sha256_hex("foobar");
    auto expected = "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2"_arr;
    EXPECT_EQ(result, expected);

    result = h5s3::hash::sha256_hex("");
    expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"_arr;
    EXPECT_EQ(result, expected);

    std::string big;
    for (int i = 0; i < 10000; ++i){
        big.append("ayy");
    }
    result = h5s3::hash::sha256_hex(big);
    expected = "a5998ec09917cd0197ce475e2f4c36e8cdcca423edd983394532fc9458d49cff"_arr;
    EXPECT_EQ(result, expected);
}

TEST(hash, hmac) {
    auto result = h5s3::hash::hmac_sha256_hexdigest("password", "foobar");
    auto expected = "eb0167a1ebf205510baff5da1465537944225f0e0140e1880b746f361ff11dca"_arr;
    EXPECT_EQ(result, expected);

    result = h5s3::hash::hmac_sha256_hexdigest("password2", "");
    expected = "0dceb6e451afb0331bc5b193d7dc297e6ddafb6a9dd0c61787f3ef062b978313"_arr;
    EXPECT_EQ(result, expected);

    std::string big;
    for (int i = 0; i < 10000; ++i){
        big.append("ayy");
    }
    result = h5s3::hash::hmac_sha256_hexdigest("password", big);
    expected = "e4db461a6eecdccda7926df7b79a2efd136b2f43123f6e0ce3df0e511c6346f2"_arr;
    EXPECT_EQ(result, expected);
}
