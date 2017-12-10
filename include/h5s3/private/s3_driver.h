#include <unordered_set>

#include "h5s3/kv_driver.h"
#include "h5s3/private/page.h"
#include "h5s3/private/out_buffer.h"
#include "h5s3/s3.h"

namespace h5s3::s3_driver {

class s3_kv_store {
private:
    const std::string m_host;
    const bool m_use_tls;
    const std::string m_bucket;
    const std::string m_path;
    s3::notary m_notary;
    page::id m_max_page;
    std::size_t m_page_size;
    std::unordered_set<page::id> m_invalid_pages;

    s3_kv_store(const std::string& m_host,
                bool use_tls,
                const std::string& bucket,
                const std::string& path,
                const std::string& access_key,
                const std::string& secret_key,
                const std::string& region,
                const std::size_t page_size);
public:
    static const char* name;

    inline s3_kv_store(s3_kv_store&& mvfrom) noexcept
        : m_host(std::move(mvfrom.m_host)),
          m_use_tls(mvfrom.m_use_tls),
          m_bucket(std::move(mvfrom.m_bucket)),
          m_path(std::move(mvfrom.m_path)),
          m_notary(std::move(mvfrom.m_notary)),
          m_max_page(mvfrom.m_max_page),
          m_page_size(mvfrom.m_page_size) {}

    static s3_kv_store from_params(const std::string_view& uri_view,
                                   unsigned int,  // TODO: Use this?
                                   std::size_t page_size,
                                   const char* access_key,
                                   const char* secret_key,
                                   const char* region,
                                   const char* host,
                                   bool use_tls);

    inline std::size_t page_size() const {
        return m_page_size;
    }

    inline page::id max_page() const {
        return m_max_page;
    }

    void max_page(page::id max_page);

    void read(page::id page_id, utils::out_buffer& out) const;
    void write(page::id page_id, const std::string_view& data);
    void flush();
};

using s3_driver = driver::kv_driver<s3_kv_store>;
}
