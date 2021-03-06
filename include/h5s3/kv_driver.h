#pragma once

#include <limits>
#include <type_traits>

#include "H5Epublic.h"
#include "H5FDpublic.h"
#include "H5Ipublic.h"
#include "H5Ppublic.h"

#include "h5s3/private/error.h"
#include "h5s3/private/out_buffer.h"
#include "h5s3/private/page.h"
#include "h5s3/private/utils.h"

namespace h5s3::driver {
namespace detail {
template<typename F>
struct inner_kv_store_params;

template<typename F, typename... Args>
struct inner_kv_store_params<
    F(const std::string_view&, unsigned int, std::size_t, Args...)> {

    static_assert((... && std::is_pod_v<std::remove_reference_t<Args>>),
                  "Only POD types may be used in kv_store::from_params");

    struct type {
        std::size_t page_size;
        std::size_t page_cache_size;
        std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> extra;

        type(std::size_t page_size, std::size_t page_cache_size, Args... extra)
            : page_size(page_size), page_cache_size(page_cache_size), extra(extra...) {}
    };
};

template<typename kv_store>
using kv_store_params =
    typename inner_kv_store_params<decltype(kv_store::from_params)>::type;
}  // namespace detail

/** Key-value driver for hdf5.

    This driver is polymorphic over the backing key-value store.

    @tparam kv_store The type of the backing key-value store.
 */
template<typename kv_store>
struct kv_driver {
private:
    using page_table = page::table<kv_store>;
    using params_struct = detail::kv_store_params<kv_store>;

    /** The hdf5 driver class.
     */
    static H5FD_class_t m_class;

    /** The hdf5 file information. This must be the first member.
     */
    H5FD_t m_public;
    page_table m_page_table;
    haddr_t m_eoa;

    kv_driver(page_table&& table) : m_page_table(std::move(table)), m_eoa(0) {}

    /** Initialize the driver's hdf5 class. This function is idempotent.

        @return The hdf5 driver class id.
     */
    static hid_t initialize() {
        m_class.name = kv_store::name;
        m_class.maxaddr = std::numeric_limits<std::size_t>::max() - 1;
        m_class.fc_degree = H5F_CLOSE_WEAK;

        m_class.fapl_size = sizeof(params_struct);

        m_class.open = open;
        m_class.close = close;
        m_class.query = query;
        m_class.get_eoa = get_eoa;
        m_class.set_eoa = set_eoa;
        m_class.get_eof = get_eof;
        m_class.read = read;
        m_class.write = write;
        m_class.flush = flush;
        m_class.truncate = truncate;

        H5FD_mem_t fl_map[] = H5FD_FLMAP_SINGLE;
        std::memcpy(m_class.fl_map, fl_map, sizeof(fl_map) / sizeof(H5FD_mem_t));

        return H5FDregister(&m_class);
    }

    /** Open an hdf5 file using this driver.

        @param name The name passed to H5Fcreate or H5Fopen.
        @param flags The bitmask of file access flags.
        @param fapl_id The id of the file access property list which contains
                       our custom arguments.
        @return An hdf5 file or `nullptr` if an error occurred.
     */
    static H5FD_t*
    open(const char* name, unsigned int flags, hid_t fapl_id, haddr_t) noexcept {
        auto params = reinterpret_cast<const params_struct*>(H5Pget_driver_info(fapl_id));

        if (nullptr == params) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_PLIST,
                         H5E_BADVALUE,
                         "bad VFL driver info");
            return nullptr;
        }

        auto store_params =
            std::tuple_cat(std::make_tuple(name, flags, params->page_size),
                           params->extra);

        try {
            auto store = std::apply(kv_store::from_params, std::move(store_params));

            std::size_t page_cache_size = params->page_cache_size;
            if (page_cache_size == 0) {
                using utils::operator""_GB;

                page_cache_size = 4_GB / store.page_size();
            }
            page_table t(std::move(store), page_cache_size);
            kv_driver* f = new kv_driver(std::move(t));
            return reinterpret_cast<H5FD_t*>(f);
        }
        catch (const std::exception& e) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_FILE,
                         H5E_CANTOPENFILE,
                         e.what());
            return nullptr;
        }
    }

    /** Close an hdf5 file that uses this driver.

        @param file The hdf5 file to close.
     */
    static herr_t close(H5FD_t* file) noexcept {
        delete reinterpret_cast<kv_driver*>(file);
        return 0;
    }

    /** Query for features of the driver.

        @param flags The bitmask to store the output flags in.
        @return zero on success, non-zero on error.
     */
    static herr_t query(const H5FD_t*, unsigned long* flags) {
        if (flags) {
            *flags = 0;
            *flags |= H5FD_FEAT_AGGREGATE_METADATA;
            *flags |= H5FD_FEAT_ACCUMULATE_METADATA;
            *flags |= H5FD_FEAT_DATA_SIEVE;
            *flags |= H5FD_FEAT_AGGREGATE_SMALLDATA;
        }

        return 0;
    }

    /** Get the end of allocated address.

        @param file The file to retrieve the eoa on.
        @return The end of allocated address.
     */
    static haddr_t get_eoa(const H5FD_t* file, H5FD_mem_t) {
        return reinterpret_cast<const kv_driver*>(file)->m_eoa;
    }

    /** Set the end of allocated address.

        This is how memory is allocated and freed.

        @param file The file to set the eoa on.
        @param addr The new end of allocated address.
        @return zero on success, non-zero on failure.
     */
    static herr_t set_eoa(H5FD_t* file, H5FD_mem_t, haddr_t addr) noexcept {
        reinterpret_cast<kv_driver*>(file)->m_eoa = addr;
        return 0;
    }

    /** Get the end of file address.

        @param file The file to get the eof on.
        @return The end of file address.
    */
#if H5_VERSION_GE(1, 10, 0)
    static haddr_t get_eof(const H5FD_t* file, H5FD_mem_t) noexcept {
#else
    static haddr_t get_eof(const H5FD_t* file) noexcept {
#endif
        const kv_driver& d = *reinterpret_cast<const kv_driver*>(file);
        return std::max(d.m_eoa, d.m_page_table.eof());
    }

    /** Read data out of an hdf5 file.

        @param file The file to read from.
        @param addr The starting address of the read.
        @param size The size of the read
        @param buf The output buffer.
        @return zero on success, non-zero on failure.
     */
    static herr_t
    read(H5FD_t* file, H5FD_mem_t, hid_t, haddr_t addr, size_t size, void* buf) noexcept {
        const auto& table = reinterpret_cast<kv_driver*>(file)->m_page_table;
        utils::out_buffer out{reinterpret_cast<char*>(buf), size};
        try {
            table.read(addr, out);
        }
        catch (const std::exception& e) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_IO,
                         H5E_READERROR,
                         e.what());
            return -1;
        }

        return 0;
    }

    /** Write data to an hdf5 file.

        @param file The file to write to.
        @param addr The starting address of the write.
        @param size The size of the write.
        @param buf The buffer to copy from.
        @return zero on success, non-zero on failure.
    */
    static herr_t write(H5FD_t* file,
                        H5FD_mem_t,
                        hid_t,
                        haddr_t addr,
                        size_t size,
                        const void* buf) noexcept {
        auto& table = reinterpret_cast<kv_driver*>(file)->m_page_table;
        const std::string_view view(reinterpret_cast<const char*>(buf), size);
        try {
            table.write(addr, view);
        }
        catch (const std::exception& e) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_IO,
                         H5E_READERROR,
                         e.what());
            return -1;
        }

        return 0;
    }

    /** Flush data to an hdf5 file.

        @param file The file to flush.
        @return zero on success, non-zero on failure.
     */
#if H5_VERSION_GE(1, 10, 0)
    static herr_t flush(H5FD_t* file, hid_t, hbool_t) noexcept
#else
    static herr_t flush(H5FD_t* file, hid_t, unsigned int) noexcept
#endif
    {
        try {
            reinterpret_cast<kv_driver*>(file)->m_page_table.flush();
        }
        catch (const std::exception& e) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_IO,
                         H5E_WRITEERROR,
                         e.what());
            return -1;
        }

        return 0;
    }

    /** Truncate an hdf5 file.

        @return zero on success, non-zero on failure.
     */
    static herr_t truncate(H5FD_t* file, hid_t, hbool_t) noexcept {
        kv_driver& d = *reinterpret_cast<kv_driver*>(file);
        try {
            d.m_page_table.truncate(d.m_eoa);
        }
        catch (const std::exception& e) {
            error::raise(__FILE__,
                         __PRETTY_FUNCTION__,
                         __LINE__,
                         H5E_IO,
                         H5E_SEEKERROR,
                         e.what());
            return -1;
        }

        return 0;
    }

public:
    /** Set the parameters on the file access property list.

        @param fapl_id The id of the file access property list to modify.
        @param page_size The size of each page. Pass 0 for the default or
                         to read the value out of an existing file. If passed
                         when opening an existing file, it must match.
        @param page_cache_size The number of pages to hold in memory.
        @param extra The arguments to forward to the kv_store.

        @return 0 on success, -1 on failure.
     */
    template<typename... Extra>
    static herr_t set_fapl(hid_t fapl_id,
                           std::size_t page_size,
                           std::size_t page_cache_size,
                           Extra... extra) {
        params_struct params(page_size, page_cache_size, extra...);

        hid_t driver_id = initialize();
        if (driver_id < 0) {
            return -1;
        }

        // H5Pset_driver copies the params structure
        return H5Pset_driver(fapl_id, driver_id, &params);
    }
};
}  // namespace h5s3::driver
