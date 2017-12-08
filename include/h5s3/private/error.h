#pragma once

#include <cstdint>
#include <sstream>

#include "H5Ipublic.h"
#include "H5Epublic.h"

namespace h5s3::error {
/** hdf5 exception handling context.

    ## Usage

    ```
    {
        h5s3::error::context ctx(__FILE__, __PRETTY_FUNCTION__);
        // ...
        if (err) {
            ctx.raise(__LINE__,
                      H5E_MAJOR_ID,
                      H5E_MINOR_ID,
                      "expected_value != given_value (",
                      expected_value,
                      " != "
                      given_value
                      ')');
            return nullptr;
        }
    }
    ```
 */
class context final {
private:
    const char* m_file_name;
    const char* m_func_name;

public:
    /** Establish an error context for the given function.

        @param file_name The result of the macro `__FILE__`.
        @param func_name The result of the macro `__PRETTY_FUNCTION__`.
     */
    inline context(const char* file_name, const char* func_name)
        : m_file_name(file_name),
          m_func_name(func_name) {}

    /** Raise an hdf5 exception.

        @param line The result of the macro `__LINE__`.
        @param maj_id The hdf5 major exception id.
        @param min_id The hdf5 minor exception id.
        @param args Arguments to format into the exception. The objects will be
                    formatted using the std::ostream formatting protocol.
     */
    template<typename... Args>
    void raise(std::size_t line,
               hid_t maj_id,
               hid_t min_id,
               Args&&... args) {
        std::stringstream s;
        (s << ... << std::forward<Args>(args));
        std::string format_string(s.str());
        H5Epush2(H5E_DEFAULT,
                 m_file_name,
                 m_func_name,
                 line,
                 H5E_ERR_CLS_g,
                 maj_id,
                 min_id,
                 format_string.data());
    }
};
}  // namespace h5s3::error
