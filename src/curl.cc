#include "h5s3/private/curl.h"

namespace h5s3 {
namespace curl {

std::string session::get(const std::string& url) const {
    std::string out;
    std::array<char, CURL_ERROR_SIZE> error_message_buffer;
    CURL* curl(m_curl.get());

    auto write = [](char *ptr, std::size_t size, std::size_t nmemb, void* buf){
        reinterpret_cast<std::string*>(buf)->append(ptr, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl,
                     CURLOPT_WRITEFUNCTION,
                     static_cast<curl_write_callback>(write));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_message_buffer.data());

    CURLcode error_code = curl_easy_perform(curl);
    if (CURLE_OK != error_code){
        //TODO: curl_easy_strerror
        throw error(error_message_buffer.data());
    }

    long response_code;
    error_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (CURLE_OK != error_code){
        throw error(curl_easy_strerror(error_code));
    }

    if (200 != response_code) {
        std::stringstream s;
        s << "Request was not a 200. Status was " << response_code << ".";
        throw error(s.str());
    }

    return out;
}

}  // namespace curl
}  // namespace h5s3
