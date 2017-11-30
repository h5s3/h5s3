#pragma once

#include "H5FDpublic.h"

#include "h5s3/s3.h"

namespace h5s3 {
namespace driver {

struct file_access_mode {
    hsize_t block_size;
};

struct driver_data {
    H5FD_t m_public;
    s3::bucket m_s3_bucket;
    hsize_t m_block_size;
    hsize_t m_block_count;
    haddr_t m_eoa;
};
}  // namespace driver
}  // namespace h5s3
