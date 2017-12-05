#include "h5s3/private/s3_driver.h"

namespace h5s3::s3_driver {
const char* s3_kv_store::name = "h5s3";

// declare storage for the static member m_class in this TU
template<>
H5FD_class_t s3_driver::m_class{};
}  // namespace h5s3::s3_driver
