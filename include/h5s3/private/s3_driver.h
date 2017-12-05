#include "h5s3/driver.h"
#include "h5s3/private/utils.h"
#include "h5s3/s3.h"

#include <regex>

namespace h5s3::s3_driver {

class s3_kv_store {
private:
    const std::string m_bucket;
    const std::string m_path;
    s3::notary m_notary;
    driver::metadata m_metadata;

    inline s3_kv_store(const std::string& bucket,
                       const std::string& path,
                       const std::string& access_key,
                       const std::string& secret_key,
                       const std::string& region,
                       const std::size_t page_size)
        : m_bucket(bucket), m_path(path), m_notary(region, access_key, secret_key) {

        try {
            std::string result = s3::get_object(m_notary,
                                                bucket,
                                                path + "/.meta");
            std::stringstream parser(result);
            parser >> m_metadata;
        }
        catch (const curl::http_error& e) {
            if (e.code != 404) {
                throw;
            }
            m_metadata.page_size = page_size;
            m_metadata.eof = 0;
        }
    }

public:
    static const char* name;
    inline static s3_kv_store from_params(const std::string_view& uri_view,
                                          unsigned int,  // TODO: Use this?
                                          std::size_t page_size,
                                          const char* access_key,
                                          const char* secret_key,
                                          const char* region) {
        std::string uri(uri_view);
        std::regex url_regex("s3://(.+)/(.+)");
        std::smatch match;

        if (!std::regex_match(uri, match, url_regex)) {
            throw std::runtime_error(uri);
        }

        std::string bucket = match[1].str();
        std::string path = match[2].str();

        // Trim trailing slashes.
        while (path.back() == '/') {
            path.pop_back();
        }

        // TODO: Allow anonymous usage without either key.
        // TODO: Validate that these are valid keys if possible.
        if (!access_key) {
            throw std::runtime_error("Access Key is Required.");
        }
        if (!secret_key) {
            throw std::runtime_error("Secret Key is Required.");
        }
        if (!region) {
            region = "us-east-1";
        }

        if (!page_size) {
            using utils::operator""_MB;
            page_size = 2_MB;
        }

        return {bucket, path, access_key, secret_key, region, page_size};
    }

    inline s3_kv_store(s3_kv_store&& mvfrom) noexcept
        : m_bucket(std::move(mvfrom.m_bucket)),
          m_path(std::move(mvfrom.m_path)),
          m_notary(std::move(mvfrom.m_notary)),
          m_metadata(std::move(mvfrom.m_metadata)) {}

    // TODO: This probably should live in a close/flush method.
    ~s3_kv_store() {
        try {
            std::stringstream formatter;
            formatter << m_metadata;
            s3::set_object(m_notary, m_bucket, m_path + "/.meta", formatter.str());
        }
        catch (...) {}
    }

    inline driver::metadata metadata() const {
        return m_metadata;
    }

    inline driver::metadata metadata() {
        return const_cast<const s3_kv_store*>(this)->metadata();
    }

    std::unique_ptr<char[]> read(page::id page_id) const {
        std::string keyname(m_path + "/" + std::to_string(page_id));
        try {
            // TODO: Don't make a request if we know it's going to fail. (We
            //       can track the EOF and know if it should be possible to
            //       fail.)
            std::string result = s3::get_object(m_notary,
                                                m_bucket,
                                                keyname);
            auto out = std::make_unique<char[]>(result.size());
            result.copy(out.get(), result.size());
            return out;
        }
        catch(const curl::http_error& e) {
            if (e.code != 404) {
                throw;
            }
        }
        return std::make_unique<char[]>(m_metadata.page_size);
    }

    void write(page::id page_id, const std::string_view& data) {
        std::string keyname(m_path + "/" + std::to_string(page_id));
        s3::set_object(m_notary,
                       m_bucket,
                       keyname,
                       data);
        std::size_t page_size = m_metadata.page_size;
        m_metadata.eof = std::max(m_metadata.eof, page_id * page_size + page_size);
    }
};

using s3_driver = driver::kv_driver<s3_kv_store>;
}
