#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "curl/curl.h"

#include "h5s3/private/out_buffer.h"

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

class http_error : public error {
public:
    const int code;
    explicit http_error(const std::string& message, int code)
        : error(message), code(code) {}
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
    /** Perform an HTTP GET request, returning the response as a `std::string`.

        @param url The url to GET.
        @param headers The headers to set in the request.
        @return The response body.
     */
    std::string get(const std::string_view& url,
                    const std::vector<header>& headers) const;

    /** Perform and HTTP GET request, writing the response into an out_buffer.

        @param url The url to GET.
        @param headers The headers to set in the request.
        @param out The output buffer to write to.
        @return The number of bytes written to `out`.
     */
    std::size_t get(const std::string_view& url,
                    const std::vector<header>& headers,
                    utils::out_buffer& out) const;

    std::string put(const std::string_view& url,
                    const std::vector<header>& headers,
                    const std::string_view& content) const;
};
}  // namespace h5s3::curl
