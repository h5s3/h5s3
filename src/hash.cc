#include "h5s3/private/hash.h"

namespace h5s3::hash {

constexpr std::array<char, 16> hexcodes({'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'});

/** Convert a sha256 hash digest to hexadecimal.
*/
sha256_hex to_hex(const sha256& hash){
    sha256_hex result;
    for (unsigned int i = 0; i < hash.size(); ++i){
        result[2 * i] = hexcodes[(hash[i] & 0xF0) >> 4];
        result[2 * i + 1] = hexcodes[hash[i] & 0x0F];
    }
    return result;
}

/* Generate a sha256 hexdigest of `data`.
   See https://tools.ietf.org/html/rfc4634.
 */
sha256_hex sha256_hexdigest(const std::string_view& data) {
    sha256 hash;
    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash.data());
    return to_hex(hash);
}

#if OPENSSL_VERSION_NUMBER < 0x01010000L
/** RAII struct for OpenSSL HMAC_CTX.
 */
class hmac_context final {
private:
    HMAC_CTX m_ctx;
public:
    hmac_context() {
        HMAC_CTX_init(&m_ctx);
    }

    ~hmac_context() {
        HMAC_CTX_cleanup(&m_ctx);
    }

    HMAC_CTX* get() {
        return &m_ctx;
    };
};
#else
/** RAII struct for OpenSSL HMAC_CTX.
 */
class hmac_context final {
private:
    HMAC_CTX* m_ctx;
public:
    hmac_context() : m_ctx(HMAC_CTX_new()) {}

    ~hmac_context() {
        HMAC_CTX_free(m_ctx);
    }

    HMAC_CTX* get() {
        return m_ctx;
    };
};
#endif  // OPENSSL_VERSION < 0x01010000


/* Generate a sha256 HMAC hexdigest from `data`.
   See https://tools.ietf.org/html/rfc4868.
*/
sha256_hex hmac_sha256_hexdigest(const std::string_view& key,
                                 const std::string_view& data) {
    hmac_context ctx;

#if OPENSSL_VERSION_NUMBER < 0x01010000L
    int result = HMAC_Init(ctx.get(), key.data(), key.size(), EVP_sha256());
#else
    int result = HMAC_Init_ex(ctx.get(),
                              key.data(),
                              key.size(),
                              EVP_sha256(),
                              nullptr);
#endif
    if (!result) {
        throw error("Failed to init HMAC_CTX.");
    }

    if (!HMAC_Update(ctx.get(),
                     reinterpret_cast<unsigned const char*>(data.data()),
                     data.size())) {
        throw error("Failed to update HMAC_CTX.");
    }

    sha256 hash;
    unsigned int outlen;
    if (!HMAC_Final(ctx.get(), hash.data(), &outlen)){
        throw error("Failed to finalize HMAC_CTX.");
    }

    return to_hex(hash);
}
} // namespace h5s3::hash
