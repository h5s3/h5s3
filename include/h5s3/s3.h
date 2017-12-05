#include <ctime>
#include <iomanip>
#include <string>
#include <vector>

#include "h5s3/private/curl.h"
#include "h5s3/private/hash.h"

namespace h5s3::s3 {

using header = curl::header;
using query_param = curl::query_param;

enum class HTTPVerb {
    GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH,
};

class notary {
private:
    const std::string m_region;
    const std::string m_now;
    const std::string m_access_key;
    const hash::sha256 m_signing_key;

public:
    notary(const std::string& region,
           const std::string& access_key,
           const std::string& secret_key);

    const std::string& signing_time() const {
        return m_now;
    }

    // http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-authenticating-requests.html
    std::string authorization_header(const HTTPVerb verb,
                                     const std::string_view& canonical_uri,
                                     const std::vector<query_param>& query,
                                     const std::vector<header>& headers,
                                     const std::string_view& payload_hash) const;
};

std::string get_object(const notary& signer,
                       const std::string_view& bucket_name,
                       const std::string_view& path);

std::string set_object(const notary& signer,
                       const std::string_view& bucket_name,
                       const std::string_view& path,
                       const std::string_view& content);
}  // namespace h5s3::s3
