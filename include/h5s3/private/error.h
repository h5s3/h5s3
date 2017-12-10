#pragma once

#include <cstdint>
#include <sstream>

#include "H5Ipublic.h"
#include "H5Epublic.h"

namespace h5s3::error {
/** Raise an hdf5 exception.

    @param file_name The result of the macro `__FILE__`.
    @param func_name The result of the macro `__PRETTY_FUNCTION__`.
    @param line The result of the macro `__LINE__`.
    @param maj_id The hdf5 major exception id.
    @param min_id The hdf5 minor exception id.
    @param args Arguments to format into the exception. The objects will be
                formatted using the std::ostream formatting protocol.

    ## Usage

    ```
    h5s3::error::raise(__FILE__,
                       __PRETTY_FUNCTION__,
                       __LINE__,
                       H5E_MAJOR_ID,
                       H5E_MINOR_ID,
                       "expected_value != given_value (",
                       expected_value,
                       " != "
                       given_value
                       ')');
    ```
 */
template<typename... Args>
void raise(const char* file_name,
           const char* func_name,
           std::size_t line,
           hid_t maj_id,
           hid_t min_id,
           Args&&... args) {
    std::stringstream s;
    (s << ... << std::forward<Args>(args));
    std::string format_string(s.str());
    H5Epush2(H5E_DEFAULT,
             file_name,
             func_name,
             line,
             H5E_ERR_CLS_g,
             maj_id,
             min_id,
             format_string.data());
}
}  // namespace h5s3::error
