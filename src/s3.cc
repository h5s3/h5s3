#include <sstream>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "h5s3/s3.h"

namespace h5s3::s3 {

namespace detail {
std::string now_str() {
    std::time_t now = std::time(nullptr);
    std::stringstream out;
    out << std::put_time(std::gmtime(&now), "%Y%m%dT%H%M%SZ");
    return out.str();
}

const std::array<std::string, 10> HTTP_VERB_NAMES = {
    "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH",
};

template <typename E>
constexpr inline auto to_underlying(E e) {
    return static_cast<std::underlying_type_t<E>>(e);
}

const inline std::string& canonicalize_verb(const HTTPVerb verb){
    return HTTP_VERB_NAMES[to_underlying(verb)];
}
} // namespace detail

hash::sha256 calculate_signing_key(const std::string_view& now,
                                   const std::string_view& region,
                                   const std::string_view& secret_key) {
    std::stringstream date_key_formatter;
    date_key_formatter << "AWS4" << secret_key;
    auto date_key = hash::hmac_sha256(date_key_formatter.str(), now.substr(0, 8));
    auto date_region_key = hash::hmac_sha256(date_key, region);
    auto date_region_service_key = hash::hmac_sha256(date_region_key, "s3");
    auto signing_key = hash::hmac_sha256(date_region_service_key, "aws4_request");
    return signing_key;
}

notary::notary(const std::string& region,
               const std::string& access_key,
               const std::string& secret_key)
    : m_region(region),
      m_now(detail::now_str()),
      m_access_key(access_key),
      m_signing_key(calculate_signing_key(m_now, m_region, secret_key)) {};

// http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-authenticating-requests.html
std::string notary::authorization_header(const HTTPVerb verb,
                                         const std::string_view& canonical_uri,
                                         const std::vector<query_param>& query,
                                         const std::vector<header>& headers,
                                         const std::string_view& payload_hash) const {
    std::stringstream canonical_request_formatter;

    // HTTP Verb
    canonical_request_formatter << detail::canonicalize_verb(verb) << '\n';

    // URI
    canonical_request_formatter << canonical_uri << '\n';

    // Query Params
    for (const auto& [key, value] : query) {
        canonical_request_formatter << key << '=' << value;
    }
    canonical_request_formatter << '\n';

    // Headers
    for (const auto& [key, value] : headers) {
        canonical_request_formatter << key << ':' << value << '\n';
    }
    canonical_request_formatter << '\n';

    // "Signed Headers"
    std::stringstream signed_headers_formatter;
    for (const auto& kv : headers) {
        signed_headers_formatter << kv.first << ';';
    }

    std::string signed_headers(signed_headers_formatter.str());
    signed_headers.pop_back();  // Remove trailing semicolon.
    canonical_request_formatter << signed_headers << '\n';

    // Payload Hash
    canonical_request_formatter << payload_hash;

    // Full Canonical Request
    std::string canonical_request(canonical_request_formatter.str());
    auto canonical_request_hash = hash::sha256_hexdigest(canonical_request);

    // "String to Sign"
    std::stringstream string_to_sign_formatter;
    auto date = m_now.substr(0, 8);
    string_to_sign_formatter << "AWS4-HMAC-SHA256" << '\n'
                             << m_now << '\n'
                             << date << '/' << m_region << "/s3/aws4_request\n"
                             << hash::as_string_view(canonical_request_hash);
    std::string string_to_sign = string_to_sign_formatter.str();

    // Auth Signature
    auto signature = hash::hmac_sha256_hexdigest(m_signing_key, string_to_sign);

    // Auth Header
    std::stringstream out_formatter;
    out_formatter << "AWS4-HMAC-SHA256"
                  << " Credential=" << m_access_key << '/' << date << '/' << m_region << "/s3/aws4_request"
                  << ",SignedHeaders=" << signed_headers
                  << ",Signature=" << hash::as_string_view(signature);
    return out_formatter.str();
}

std::string get_bucket(const notary& signer,
                       const std::string& bucket_name,
                       const std::string& path) {

    std::stringstream host_formatter;
    host_formatter << bucket_name << ".s3.amazonaws.com";
    std::string hostname = host_formatter.str();

    hash::sha256_hex payload_hash = hash::sha256_hexdigest("");

    std::vector<query_param> query = {};
    std::vector<header> headers = {
        {"host", hostname},
        {"x-amz-content-sha256", hash::as_string_view(payload_hash)},
        {"x-amz-date", signer.signing_time()},
    };

    std::string auth = signer.authorization_header(HTTPVerb::GET,
                                                   path,
                                                   query,
                                                   headers,
                                                   hash::as_string_view(payload_hash));
    headers.emplace_back("Authorization", auth);

    std::stringstream url_formatter;
    url_formatter << "https://" << bucket_name << ".s3.amazonaws.com" << path;

    curl::session session;
    return session.get(url_formatter.str(), headers);
}

std::string set_bucket(const notary& signer,
                       const std::string& bucket_name,
                       const std::string& path,
                       const std::string& content) {

    std::stringstream host_formatter;
    host_formatter << bucket_name << ".s3.amazonaws.com";
    std::string hostname = host_formatter.str();

    hash::sha256_hex payload_hash = hash::sha256_hexdigest(content);
    const std::string& signing_time = signer.signing_time();

    std::vector<query_param> query = {};
    std::vector<header> headers = {
        {"host", hostname},
        {"x-amz-content-sha256", hash::as_string_view(payload_hash)},
        {"x-amz-date", signing_time}
    };

    std::string auth = signer.authorization_header(HTTPVerb::PUT,
                                                   path,
                                                   query,
                                                   headers,
                                                   hash::as_string_view(payload_hash));
    headers.emplace_back("Authorization", auth);

    std::stringstream url_formatter;
    url_formatter << "https://" << bucket_name << ".s3.amazonaws.com" << path;

    curl::session session;
    return session.put(url_formatter.str(), headers, content);
}

}  // namespace h5s3::s3
