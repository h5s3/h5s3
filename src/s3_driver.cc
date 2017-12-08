#include "h5s3/private/s3_driver.h"

namespace h5s3::s3_driver {
const char* s3_kv_store::name = "h5s3";

s3_kv_store::s3_kv_store(const std::string& bucket,
                         const std::string& path,
                         const std::string& access_key,
                         const std::string& secret_key,
                         const std::string& region,
                         const std::size_t page_size)
    : m_bucket(bucket),
      m_path(path),
      m_notary(region, access_key, secret_key) {

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

s3_kv_store s3_kv_store::from_params(const std::string_view& uri_view,
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


std::unique_ptr<char[]> s3_kv_store::read(page::id page_id) const {
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

void s3_kv_store::write(page::id page_id, const std::string_view& data) {
    std::string keyname(m_path + "/" + std::to_string(page_id));
    s3::set_object(m_notary,
                   m_bucket,
                   keyname,
                   data);
    std::size_t page_size = m_metadata.page_size;
    m_metadata.eof = std::max(m_metadata.eof, page_id * page_size + page_size);
}

void s3_kv_store::flush() {
    std::stringstream formatter;
    formatter << m_metadata;
    s3::set_object(m_notary, m_bucket, m_path + "/.meta", formatter.str());
}

void s3_kv_store::truncate() {
    // TODO: delete the pages

    m_metadata.eof = 0;
    flush();
}

// declare storage for the static member m_class in this TU
template<>
H5FD_class_t s3_driver::m_class{};
}  // namespace h5s3::s3_driver
