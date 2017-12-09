#include <regex>

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
      m_notary(region, access_key, secret_key),
      m_max_page(0),
      m_page_size(page_size) {

    try {
        std::string result = s3::get_object(m_notary,
                                            bucket,
                                            path + "/.meta");

        std::regex metadata_regex("page_size=([0-9]+)\n"
                                  "max_page=([0-9]+)\n"
                                  "invalid_pages={(([0-9]+ )*[0-9]*)}\n");
        std::smatch match;

        if (!std::regex_match(result, match, metadata_regex)) {
            std::stringstream s;
            s << "failed to parse metadata from .meta file: "
              << result;
            throw std::runtime_error(s.str());
        }

        {
            std::size_t metadata_page_size;
            std::stringstream s(match[1].str());
            s >> metadata_page_size;
            if (m_page_size != 0 && metadata_page_size != m_page_size) {
                std::stringstream s;
                s << "passed page size does not match existing page size: "
                  << m_page_size << " != " << metadata_page_size;
                throw std::runtime_error(s.str());
            }

            m_page_size = metadata_page_size;
        }

        {
            std::stringstream s(match[2].str());
            s >> m_max_page;
        }

        {
            std::stringstream s(match[3].str());
            page::id page_id;
            while (s >> page_id) {
                m_invalid_pages.insert(page_id);
            }
        }
    }
    catch (const curl::http_error& e) {
        if (e.code != 404) {
            throw;
        }
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

void s3_kv_store::max_page(page::id max_page) {
    if (max_page < m_max_page) {
        for (page::id page_id = max_page + 1;
             page_id <= m_max_page;
             ++page_id) {
            m_invalid_pages.insert(page_id);
        }
    }

    m_max_page = max_page;
}

std::unique_ptr<char[]> s3_kv_store::read(page::id page_id) const {
    if (page_id > m_max_page ||
        m_invalid_pages.find(page_id) != m_invalid_pages.end()) {
        return new_page();
    }

    std::string keyname(m_path + "/" + std::to_string(page_id));
    try {
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
    return new_page();
}

void s3_kv_store::write(page::id page_id, const std::string_view& data) {
    std::string keyname(m_path + "/" + std::to_string(page_id));
    s3::set_object(m_notary,
                   m_bucket,
                   keyname,
                   data);
    m_max_page = std::max(m_max_page, page_id);
    m_invalid_pages.erase(page_id);
}

void s3_kv_store::flush() {
    std::stringstream formatter;
    formatter << "page_size=" << m_page_size << '\n'
              << "max_page=" << m_max_page << '\n'
              << "invalid_pages={";
    for (page::id page_id : m_invalid_pages) {
        formatter << page_id << ',';
    }
    if (m_invalid_pages.size()) {
        // consume the trailing comma
        formatter.get();
    }
    s3::set_object(m_notary, m_bucket, m_path + "/.meta", formatter.str());
}

// declare storage for the static member m_class in this TU
template<>
H5FD_class_t s3_driver::m_class{};
}  // namespace h5s3::s3_driver
