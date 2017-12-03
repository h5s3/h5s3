#pragma once

#include <sstream>
#include <type_traits>

#include "H5FDpublic.h"
#include "H5Ppublic.h"
#include "H5Ipublic.h"

#include "h5s3/private/page.h"

namespace h5s3::driver {

struct metadata {
    std::size_t page_size;
    std::size_t eof;
};

namespace detail {
template<typename F>
struct inner_kv_store_params;

template<typename F, typename... Args>
struct inner_kv_store_params<F(const std::string_view&,
                               std::size_t,
                               unsigned int,
                               Args...)> {
    static_assert(std::conjunction_v<
                  std::is_pod_v<
                  std::remove_cv_t<std::remove_reference_t<Args>>>...>,
                  "Only POD types may be used in kv_store::from_params");

    struct type {
        std::size_t page_size;
        std::size_t page_cache_size;
        std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> extra;
    };

};

template<typename kv_store>
using kv_store_params = inner_kv_store_params<decltype(kv_store::from_params)>;
}  // namespace detail

/** Key-value driver for hdf5.

    This driver is polymorphic over the backing key-value store.

    @tparam kv_store The type of the backing key-value store.

    ## Notes
    `kv_store` must implement the following methods:

    ```
    // construct a kv_store from the params passed to H5create
    static kv_store from_params(const std::string_view& path,
                                std::size_t page_size,
                                unsigned int flags,
                                Args...);

    // read the metadata from the store if the file already exists
    std::optional<metadata> metadata();

    // write the metadata to the store
    void metadata(const metadata&);

    // read the contents of a page; this should allocate a new page
    // of zeroed data if needed
    std::unique_ptr<char[]> read(page::id page_id);

    // write the buffer to a page
    void write(page::id, const std::string_view& data);
 */
template<typename kv_store>
struct kv_driver {
private:
    using page_table = page::table<kv_store>;
    using params_struct = detail::kv_store_params<kv_store>;

    H5FD_t m_public;
    page_table m_page_table;
    haddr_t m_eoa;
    haddr_t m_eof;

    kv_driver(page_table&& table, haddr_t eof)
        : m_page_table(std::move(table)),
          m_eoa(0),
          m_eof(eof) {}

public:
    /** Set the parameters on the file access property list.

        @param page_size The size of each page. Pass 0 for the default or
                         to read the value out of an existing file. If passed
                         when opening an existing file, it must match.
        @param page_cache_size The number of pages to hold in memory.
        @param extra 
     */
    /** hdf5 driver open implenentation.

        @param name The name passed to H5Fcreate or H5Fopen.
        @param flags The bitmask of file access flags.
        @param fapl_id The id of the file access property list which contains
                       our custom arguments.
        @return An hdf5 file or `nullptr` if an error occurred.
     */
    static H5FD_t* open(const char* name,
                        unsigned int flags,
                        hid_t fapl_id,
                        haddr_t) {
        params_struct* params = reinterpret_cast<params_struct*>(H5Pget_driver_info(fapl_id));
        if (nullptr == params) {
            return nullptr;
        }

        auto store_params = std::tuple_cat(std::make_tuple(name,
                                                           flags,
                                                           params->page_size),
                                           params->extra);

        kv_store store;
        try {
            store = std::apply(kv_store::from_params, std::move(store_params));
        }
        catch (const std::exception&) {
            return nullptr;
        }

        std::size_t eof = store.metadata().eof;
        page_table t(std::move(store), std::max(params->page_cache_size, 1));

        kv_driver* f = new kv_driver(std::move(t), eof);

        return reinterpret_cast<H5FD_t*>(f);
    }

    static herr_t query(const H5FD_t*, unsigned long* flags) {
        if(flags) {
            *flags = 0;
            *flags |= H5FD_FEAT_AGGREGATE_METADATA;
            *flags |= H5FD_FEAT_ACCUMULATE_METADATA;
            *flags |= H5FD_FEAT_DATA_SIEVE;
            *flags |= H5FD_FEAT_AGGREGATE_SMALLDATA;
            *flags |= H5FD_FEAT_POSIX_COMPAT_HANDLE;
        }

        return 0;
    }

    static haddr_t get_eoa(const H5FD_t *file, H5FD_mem_t) {
        return reinterpret_cast<const kv_driver*>(file)->m_eoa;
    }

    static herr_t set_eoa(H5FD_t *file, H5FD_mem_t, haddr_t addr) {
        reinterpret_cast<kv_driver*>(file)->m_eoa = addr;
        kv_driver& d = *reinterpret_cast<kv_driver*>(file);
        d.m_eoa = addr;
        if (addr > d.m_eof) {
            d.m_eof = addr;
        }
        return 0;
    }

    static haddr_t get_eof(const H5FD_t *file, H5FD_mem_t) {
        const kv_driver& d = *reinterpret_cast<const kv_driver*>(file);
        return std::max(d.m_eoa, d.m_eof);
    }

    static herr_t read(H5FD_t *file,
            H5FD_mem_t,
            hid_t,
            haddr_t addr,
            size_t size,
            void *buf) {

        const auto& table = reinterpret_cast<kv_driver*>(file)->m_page_table;
        try {
            table.read(addr, size, reinterpret_cast<char*>(buf));
        }
        catch (const std::exception&) {
            return -1;
        }

        return 0;
    }

    static herr_t write(H5FD_t *file,
                        H5FD_mem_t,
                        hid_t,
                        haddr_t addr,
                        size_t size,
                        const void *buf) {
        auto& table = reinterpret_cast<kv_driver*>(file)->m_page_table;
        const std::string_view view(reinterpret_cast<const char*>(buf), size);
        try {
            table.write(addr, view);
        }
        catch (const std::exception&) {
            return -1;
        }
    }

    static herr_t truncate(H5FD_t *, hid_t, hbool_t) {
        return 0;
    }
};

extern "C" hid_t h5s3_init();

extern "C" herr_t h5s3_set_fapl(hid_t fapl_id,
                                std::size_t page_size,
                                std::size_t page_cache_size);
}  // namespace h5s3::driver
