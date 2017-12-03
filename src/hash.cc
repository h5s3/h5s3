#include "h5s3/private/hash.h"

namespace h5s3::hash {

/** Convert a sha256 hash digest to hexadecimal.
*/
sha256_hexdigest to_hex(const sha256& hash){
    sha256_hexdigest result;
    for(unsigned int i = 0; i < hash.size(); ++i){
        result[2 * i] = hexcodes[(hash[i] & 0xF0) >> 4];
        result[2 * i + 1] = hexcodes[hash[i] & 0x0F];
    }
    return result;
}

/* Generate a sha256 hexdigest of `data`.
   See https://tools.ietf.org/html/rfc4634.
 */
sha256_hexdigest sha256_hex(const std::string_view& data) {
    sha256 hash;
    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash.data());
    return to_hex(hash);
}

/** RAII struct for OpenSSL HMAC_CTX.
 */
struct hmac_context {
private:
    HMAC_CTX m_ctx;
public:
    hmac_context(){
        HMAC_CTX_init(&m_ctx);
    }

    ~hmac_context(){
        HMAC_CTX_cleanup(&m_ctx);
    }

    HMAC_CTX* get(){
        return &m_ctx;
    };
};

/* Generate a sha256 HMAC hexdigest from `data`.
   See https://tools.ietf.org/html/rfc4868.
*/
sha256_hexdigest hmac_sha256_hexdigest(const std::string_view& key,
                                       const std::string_view& data){
    hmac_context ctx;

    if (!HMAC_Init(ctx.get(), key.data(), key.size(), EVP_sha256())){
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
} // h5s3::hash
