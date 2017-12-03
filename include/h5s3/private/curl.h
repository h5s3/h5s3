#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "curl/curl.h"

namespace h5s3::curl {

typedef std::pair<std::string_view, std::string_view> header;
typedef std::pair<std::string_view, std::string_view> query_param;

class curl_deleter {
public:
    void operator()(CURL* ptr) {
        // TODO: check if we need to free ptr here.
        curl_easy_cleanup(ptr);
    }
};

class error : public std::runtime_error {
public:
    explicit error(const std::string& message) : std::runtime_error(message) {}
};

class session {
private:
    std::unique_ptr<CURL, curl_deleter> m_curl;

public:
    session() : m_curl(curl_easy_init()) {
        if (!m_curl) {
            throw error("Failed to initialize curl request.");
        }
    }
    std::string get(const std::string_view& url,
                    const std::vector<header>& headers) const;

    std::string put(const std::string_view& url,
                    const std::vector<header>& headers,
                    const std::string_view& content) const;
};
}  // namespace h5s3::curl
