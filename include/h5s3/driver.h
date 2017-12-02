#pragma once

#include <fstream>
#include <sstream>

#include "H5FDpublic.h"
#include "H5Ipublic.h"

#include "h5s3/private/page.h"

namespace h5s3 {
namespace driver {

template<typename page_table>
struct driver {
private:
    H5FD_t m_public;
    page_table m_page_table;
    std::string m_metadata_file_path;
    haddr_t m_eoa;
    haddr_t m_eof;

    driver(page_table&& table, std::string&& metadata_file_path, haddr_t eoa)
        : m_page_table(std::move(table)),
          m_metadata_file_path(std::move(metadata_file_path)),
          m_eoa(eoa),
          // initialize eof to eoa
          m_eof(eoa) {}

public:
    static H5FD_t* open(const char* name,
                        unsigned int flags,
                        hid_t,
                        haddr_t maxaddr) {
        std::string metadata_file_path(name);
        std::fstream metadata_file(metadata_file_path,
                                   metadata_file.in | metadata_file.out);
        if (!metadata_file.is_open()) {
            return nullptr;
        }

        haddr_t eoa = 0;
        metadata_file >> eoa;

        constexpr std::size_t page_size = 64;
        auto read_page = [](std::size_t page_id) {
            std::stringstream s;
            s << "/tmp/pages/" << page_id;
            auto str = s.str();
            auto f = std::fopen(str.c_str(), "r");
            auto out = std::make_unique<char[]>(page_size);
            if (!f) {
                return out;
            }
            fread(out.get(), page_size, 1, f);
            fclose(f);
            return out;
        };
        auto write_page = [](std::size_t page_id, const std::string_view& d) {
            std::stringstream s;
            s << "/tmp/pages/" << page_id;
            auto str = s.str();
            auto f = std::fopen(str.c_str(), "w");
            fwrite(d.data(), d.size(), 1, f);
            fclose(f);
        };

        page_table t(read_page, write_page, page_size, 4);
        driver* f = new driver(std::move(t),
                               std::move(metadata_file_path),
                               eoa);

        return reinterpret_cast<H5FD_t*>(f);
    }

    static herr_t close(H5FD_t* file) {
        driver* d = reinterpret_cast<driver*>(file);
        d->m_page_table.flush();
        std::fstream metadata_file(d->m_metadata_file_path, metadata_file.out);
        if (!metadata_file.is_open()) {
            return -1;
        }
        metadata_file << d->m_eoa;
        delete d;
        return 0;
    }

    static int cmp(const H5FD_t* a, const H5FD_t* b) {
        if (a < b) {
            return -1;
        }
        if (a == b) {
            return 0;
        }

        return 1;
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
        return reinterpret_cast<const driver*>(file)->m_eoa;
    }

    static herr_t set_eoa(H5FD_t *file, H5FD_mem_t, haddr_t addr) {
        reinterpret_cast<driver*>(file)->m_eoa = addr;
        driver& d = *reinterpret_cast<driver*>(file);
        d.m_eoa = addr;
        if (addr > d.m_eof) {
            d.m_eof = addr;
        }
        return 0;
    }

    static haddr_t get_eof(const H5FD_t *file, H5FD_mem_t) {
        const driver& d = *reinterpret_cast<const driver*>(file);
        return std::max(d.m_eoa, d.m_eof);
    }

    static herr_t read(H5FD_t *file,
            H5FD_mem_t,
            hid_t,
            haddr_t addr,
            size_t size,
            void *buf) {

        const auto& table = reinterpret_cast<driver*>(file)->m_page_table;
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
        auto& table = reinterpret_cast<driver*>(file)->m_page_table;
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

extern "C" herr_t h5s3_set_fapl(hid_t fapl_id);
}  // namespace driver
}  // namespace h5s3
