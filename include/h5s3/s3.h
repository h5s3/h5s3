#pragma once

#include <string>

#include "h5s3/private/curl.h"

namespace h5s3 {
namespace s3 {
struct bucket {
private:
    const std::string m_name;
public:
    bucket(const std::string& name) : m_name(name) {};
};

using error = curl::error;
}  // namespace s3
}  // namespace h5s3
