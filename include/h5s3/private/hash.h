#include "openssl/sha.h"
#include "openssl/hmac.h"
#include <array>

namespace h5s3::hash {

typedef std::array<unsigned char, SHA256_DIGEST_LENGTH> sha256;
typedef std::array<unsigned char, SHA256_DIGEST_LENGTH * 2> sha256_hex;

class error : public std::runtime_error {
public:
    explicit error(const std::string& message) : std::runtime_error(message) {}
};

sha256_hex sha256_hexdigest(const std::string_view& data);
sha256_hex hmac_sha256_hexdigest(const std::string_view& key,
                                 const std::string_view& data);
}  // h5s3::hash