#pragma once
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "curl/curl.h"

#include "h5s3/private/out_buffer.h"

namespace h5s3::curl {

using header = std::pair<std::string_view, std::string_view>;
using query_param = std::pair<std::string_view, std::string_view>;

class curl_deleter {
public:
    void operator()(CURL* ptr) {
        curl_easy_cleanup(ptr);
    }
};

class curl_slist_deleter {
public:
    void operator()(curl_slist* ptr) {
        curl_slist_free_all(ptr);
    }
};
using owned_header_list = std::unique_ptr<curl_slist, curl_slist_deleter>;

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
