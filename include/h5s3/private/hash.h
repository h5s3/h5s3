#include "openssl/sha.h"
#include "openssl/hmac.h"
#include <array>

namespace h5s3::hash {

typedef std::array<unsigned char, SHA256_DIGEST_LENGTH> sha256;
typedef std::array<unsigned char, SHA256_DIGEST_LENGTH * 2> sha256_hex;

constexpr std::array<char, 16> hexcodes({'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'});

class error : public std::runtime_error {
public:
    explicit error(const std::string& message) : std::runtime_error(message) {}
};

/** Convert a sha256 hash digest to hexadecimal.
*/
inline sha256_hex to_hex(const sha256& hash){
    sha256_hex result;
    for (unsigned int i = 0; i < hash.size(); ++i){
        result[2 * i] = hexcodes[(hash[i] & 0xF0) >> 4];
        result[2 * i + 1] = hexcodes[hash[i] & 0x0F];
    }
    return result;
}

inline std::string_view as_string_view(const sha256& hash){
    return std::string_view(reinterpret_cast<const char *>(hash.data()), hash.size());
}

inline std::string_view as_string_view(const sha256_hex& hash){
    return std::string_view(reinterpret_cast<const char *>(hash.data()), hash.size());
}

sha256 hmac_sha256(const std::string_view& key,
                   const std::string_view& data);

inline sha256 hmac_sha256(const sha256& key,
                          const std::string_view& data) {
    return hmac_sha256(as_string_view(key), data);
}

inline sha256 hmac_sha256(const sha256_hex& key,
                          const std::string_view& data) {
    return hmac_sha256(as_string_view(key), data);
}

template<typename ...Params>
sha256_hex hmac_sha256_hexdigest(Params&&... params){
    return to_hex(hmac_sha256(std::forward<Params>(params)...));
}

sha256_hex sha256_hexdigest(const std::string_view& data);

}  // h5s3::hash
