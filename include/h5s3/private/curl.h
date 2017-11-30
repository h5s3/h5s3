#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "curl/curl.h"

namespace h5s3 {
namespace curl {

class curl_deleter {
public:
    void operator()(CURL* ptr) {
        // TODO: check if we need to free ptr here.
        curl_easy_cleanup(ptr);
    }
};

class error : public std::runtime_error {
public:
    error(const std::string& message) : std::runtime_error(message) {}
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
    std::string get(const std::string& url) const;
};
}  // namespace curl
}  // namespace h5s3
