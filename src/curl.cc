#include <cstring>

#include "h5s3/private/curl.h"

namespace h5s3::curl {

void set_headers(CURL* curl, const std::vector<header>& headers){
    struct curl_slist *header_list = NULL;

    for (const header& h : headers){
        std::stringstream s;
        s << h.first << ":" << h.second;

        // TODO: Handle NULL here.
        header_list = curl_slist_append(header_list, s.str().data());

        if (!header_list) {
            throw error("Failed to construct request headers.");
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
}

namespace {

size_t read_callback(char *ptr, std::size_t size, std::size_t nmemb, void* inbuf){
    std::string_view *buf = reinterpret_cast<std::string_view*>(inbuf);

    // TODO: Should we be worried about overflow in the multiplication here?
    size_t to_copy = std::min(buf->size(), size * nmemb);
    buf->copy(ptr, to_copy);
    buf->remove_prefix(to_copy);
    return to_copy;
};

using write_callback_type = std::size_t (*)(char*,
                                            std::size_t,
                                            std::size_t,
                                            void*);

void set_common_request_fields_str(CURL *curl,
                                   const std::string_view& url,
                                   const std::vector<header>& headers,
                                   std::string& output_buffer,
                                   std::array<char, CURL_ERROR_SIZE>& error_message_buffer) {
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_message_buffer.data());

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_buffer);
    write_callback_type write_callback = [](char* ptr,
                                            std::size_t size,
                                            std::size_t nmemb,
                                            void* closure) {
        reinterpret_cast<std::string*>(closure)->append(ptr, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    set_headers(curl, headers);
}

void set_common_request_fields_out_buffer(CURL *curl,
                                          const std::string_view& url,
                                          const std::vector<header>& headers,
                                          utils::out_buffer& output_buffer,
                                          std::array<char, CURL_ERROR_SIZE>& error_message_buffer) {
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_message_buffer.data());

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_buffer);

    write_callback_type write_callback = [](char* ptr,
                                            std::size_t size,
                                            std::size_t nmemb,
                                            void* closure) {
        auto& buf = *reinterpret_cast<utils::out_buffer*>(closure);
        std::size_t write_size = size * nmemb;
        if (write_size > buf.size()) {
            throw std::out_of_range("out of bounds write");
        }
        std::memcpy(buf.data(), ptr, write_size);
        buf.remove_prefix(write_size);
        return write_size;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    set_headers(curl, headers);
}

long perform_request(CURL* curl,
                     const std::array<char, CURL_ERROR_SIZE>& error_message_buffer) {

    CURLcode error_code = curl_easy_perform(curl);

    if (CURLE_OK != error_code){
        // TODO: curl_easy_strerror
        throw error(error_message_buffer.data());
    }

    long response_code;
    error_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (CURLE_OK != error_code){
        throw error(curl_easy_strerror(error_code));
    }

    return response_code;
}

void throw_for_status(long code, const std::string_view& response_body) {
    if (200 == code) {
        return;
    }

    std::stringstream s;
    s << "Request was not a 200. Status was 400:\n" << response_body;
    throw http_error(s.str(), code);
}
}  // namespace

std::string session::get(const std::string_view& url,
                         const std::vector<header>& headers) const {
    std::string out;
    std::array<char, CURL_ERROR_SIZE> error_buffer;

    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L);

    set_common_request_fields_str(m_curl.get(),
                                  url,
                                  headers,
                                  out,
                                  error_buffer);

    long code = perform_request(m_curl.get(), error_buffer);
    throw_for_status(code, out);

    return out;
}

std::size_t session::get(const std::string_view& url,
                         const std::vector<header>& headers,
                         utils::out_buffer& out) const {
    std::array<char, CURL_ERROR_SIZE> error_buffer;

    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L);

    utils::out_buffer copy(out);

    set_common_request_fields_out_buffer(m_curl.get(),
                                         url,
                                         headers,
                                         copy,
                                         error_buffer);

    long code = perform_request(m_curl.get(), error_buffer);
    throw_for_status(code, {out.data(), out.size()});

    return copy.data() - out.data();
}

std::string session::put(const std::string_view& url,
                         const std::vector<header>& headers,
                         const std::string_view& body) const {
    std::string out;
    std::array<char, CURL_ERROR_SIZE> error_buffer;
    std::string_view body_copy = body;

    curl_easy_setopt(m_curl.get(), CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_curl.get(), CURLOPT_READFUNCTION, &read_callback);
    curl_easy_setopt(m_curl.get(), CURLOPT_READDATA, &body_copy);
    curl_easy_setopt(m_curl.get(),
                     CURLOPT_INFILESIZE_LARGE,
                     static_cast<curl_off_t>(body.size()));

    set_common_request_fields_str(m_curl.get(),
                                  url,
                                  headers,
                                  out,
                                  error_buffer);

    long code = perform_request(m_curl.get(), error_buffer);
    throw_for_status(code, out);

    return out;
}

}  // namespace h5s3::curl
