#pragma once

#include <string>

#include "hdf5.h"

/** Simple hdf5 wrapper.
 */
namespace h5s3::h5 {
class context final {
public:
    inline context() {
        H5open();
    }

    inline ~context() {
        H5close();
    }
};

class error : public std::exception {
    inline const char* what() const noexcept {
        return "h5 failure";
    }
};

class hid_wrapper {
protected:
    hid_t m_hid;

public:
    inline hid_wrapper(hid_t hid) : m_hid(hid) {
        if (hid < 0) {
            throw error();
        }
    }

    inline hid_t get() const {
        return m_hid;
    }
};

class fapl final : public hid_wrapper {
public:
    inline fapl() : hid_wrapper(H5Pcreate(H5P_FILE_ACCESS)) {}

    inline ~fapl() {
        if (m_hid >= 0) {
            H5Pclose(m_hid);
        }
    }
};

class file final : public hid_wrapper {
protected:
    inline file(hid_t hid) : hid_wrapper(hid) {}
public:
    inline static file create(const std::string& path,
                              int flags,
                              hid_t fcpl_id = H5P_DEFAULT,
                              hid_t fapl_id = H5P_DEFAULT) {
        return {H5Fcreate(path.data(), flags, fcpl_id, fapl_id)};
    }

    inline static file open(const std::string& path,
                            int flags,
                            hid_t fapl_id = H5P_DEFAULT) {
        return {H5Fopen(path.data(), flags, fapl_id)};
    }

    inline ~file() {
        if (m_hid >= 0) {
            H5Fclose(m_hid);
        }
    }
};
}  // namespace h5s3::h5
