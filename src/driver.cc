#include <limits>

#include "H5Ppublic.h"

#include "h5s3/driver.h"

namespace h5s3 {
namespace driver {

typedef std::unique_ptr<char[]> (*read_page_callback_type)(std::size_t);
typedef void (*write_page_callback_type)(std::size_t, const std::string_view&);

using driver_type = driver<page::table<read_page_callback_type,
                                       write_page_callback_type>>;

H5FD_class_t s3_driver{};

extern "C" hid_t h5s3_init() {
    s3_driver.name = "s3";
    s3_driver.maxaddr = std::numeric_limits<std::size_t>::max() - 1;
    s3_driver.fc_degree = H5F_CLOSE_WEAK;

    s3_driver.open = driver_type::open;
    s3_driver.close = driver_type::close;
    s3_driver.cmp = driver_type::cmp;
    s3_driver.query = driver_type::query;
    s3_driver.get_eoa = driver_type::get_eoa;
    s3_driver.set_eoa = driver_type::set_eoa;
    s3_driver.get_eof = driver_type::get_eof;
    s3_driver.read = driver_type::read;
    s3_driver.write = driver_type::write;
    s3_driver.truncate = driver_type::truncate;

    H5FD_mem_t fl_map[] = H5FD_FLMAP_SINGLE;
    std::memcpy(s3_driver.fl_map, fl_map, sizeof(fl_map) / sizeof(H5FD_mem_t));

    return H5FDregister(&s3_driver);
}

extern "C" herr_t h5s3_set_fapl(hid_t fapl_id) {
    H5Pset_driver(fapl_id, h5s3_init(), nullptr);
    return 0;
}

}  // namespace driver
}  // namespace h5s3
