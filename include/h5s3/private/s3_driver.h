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

    s3_kv_store(const std::string& bucket,
                const std::string& path,
                const std::string& access_key,
                const std::string& secret_key,
                const std::string& region,
                const std::size_t page_size);

public:
    static const char* name;

    inline s3_kv_store(s3_kv_store&& mvfrom) noexcept
        : m_bucket(std::move(mvfrom.m_bucket)),
          m_path(std::move(mvfrom.m_path)),
          m_notary(std::move(mvfrom.m_notary)),
          m_metadata(std::move(mvfrom.m_metadata)) {}

    static s3_kv_store from_params(const std::string_view& uri_view,
                                   unsigned int,  // TODO: Use this?
                                   std::size_t page_size,
                                   const char* access_key,
                                   const char* secret_key,
                                   const char* region);

    inline driver::metadata metadata() const {
        return m_metadata;
    }

    inline driver::metadata metadata() {
        return const_cast<const s3_kv_store*>(this)->metadata();
    }

    std::unique_ptr<char[]> read(page::id page_id) const;
    void write(page::id page_id, const std::string_view& data);
    void flush();
    void truncate();
};

using s3_driver = driver::kv_driver<s3_kv_store>;
}
