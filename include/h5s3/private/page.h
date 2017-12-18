#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "h5s3/private/out_buffer.h"

namespace h5s3::page {

/** The identifier for a given memory page.
 */
using id = std::size_t;

/** A page table adapts a `kv_store` to present the abstraction of a contiguous
    memory space. The page table implements caching to reduce the trips to the
    underlying `kv_store`.

    @tparam kv_store The underlying key-value store.
 */
template<typename kv_store>
class table {
private:
    class page;

    mutable kv_store m_kv_store;

    const std::size_t m_page_cache_size;
    using list_type = std::list<std::tuple<id, page>>;
    mutable list_type m_lru_order;
    mutable std::size_t m_allocated_pages;
    mutable std::unordered_map<id, typename list_type::iterator> m_page_cache;

    class page {
    private:
        bool m_dirty;
        bool m_zero_on_use;
        std::unique_ptr<char[]> m_data;

    public:
        page(std::size_t page_size)
            : m_dirty(false),
              m_zero_on_use(false),
              m_data(std::make_unique<char[]>(page_size)) {}

        page(page&& mvfrom) noexcept
            : m_dirty(mvfrom.m_dirty),
              m_data(std::move(mvfrom.m_data)) {}

        page& operator=(page&& mvfrom) noexcept {
            m_dirty = mvfrom.m_dirty;
            m_data = std::move(mvfrom.m_data);
            return *this;
        }

        void reset() {
            m_zero_on_use = false;
            m_dirty = false;
        }

        void invalidate() {
            m_zero_on_use = true;
            m_dirty = false;
        }

        void read(std::size_t addr,
                  utils::out_buffer& buffer,
                  std::size_t page_size) {
            if (m_zero_on_use) {
                std::memset(m_data.get(), 0, page_size);
                m_zero_on_use = false;
            }

            std::memcpy(buffer.data(), &m_data[addr], buffer.size());
        }

        void write(std::size_t addr,
                   const std::string_view& data,
                   std::size_t page_size) {
            if (m_zero_on_use) {
                std::memset(m_data.get(), 0, addr);
                std::memset(m_data.get() + data.size(),
                            0,
                            page_size - addr + data.size());
                m_zero_on_use = false;
            }
            std::memcpy(&m_data[addr], data.data(), data.size());

            // mark that this page is dirty
            m_dirty = true;
        }

        const char* data() const {
            return m_data.get();
        }

        char* data() {
            return m_data.get();
        }

        void dirty(bool dirty) {
            m_dirty = dirty;
        }

        bool dirty() const {
            return m_dirty;
        }
    };

    std::size_t page_size() const {
        return m_kv_store.page_size();
    }

    /** Read a page by first looking in the cache, then falling back to
        `m_kv_store`.

        @param page_id The page id to read.
        @return A reference to the given page.
     */
    page& read_page(id page_id) const {
        auto search = m_page_cache.find(page_id);
        if (search != m_page_cache.end()) {
            if (search->second != m_lru_order.begin()) {
                m_lru_order.splice(m_lru_order.begin(),
                                   m_lru_order,
                                   search->second,
                                   std::next(search->second));
            }
            return std::get<1>(*search->second);
        }

        if (m_page_cache.size() == m_page_cache_size) {
            // The cache is full, we are going to steal the buffer used for
            // the least recently used page.
            const auto& [to_evict, page] = m_lru_order.back();
            if (page.dirty()) {
                m_kv_store.write(to_evict,
                                 std::string_view(page.data(), page_size()));
            }
            // remove the page from the cache mapping
            m_page_cache.erase(to_evict);

            // reset the page and change the node's id to the new page id
            auto& [id, p] = m_lru_order.back();
            id = page_id;
            p.reset();
        }
        else {
            // The cache is not yet full. Allocate a new page at the end of
            // the lru list. We allocate this at the end because the read may
            // fail and we want to reuse the page as early as possible.
            m_lru_order.emplace_back(page_id, page_size());
            ++m_allocated_pages;
        }

        // Fill the page from the kv_store. Note: `m_kv_store.read` MAY throw
        // an exception and fail. If that happens, we do not want to move the
        // page node to the front of the lru order or add the entry to the
        // cache.
        page& p = std::get<1>(m_lru_order.back());
        utils::out_buffer b{p.data(), page_size()};
        m_kv_store.read(page_id, b);

        if (m_allocated_pages > 1) {
            m_lru_order.splice(m_lru_order.begin(),
                               m_lru_order,
                               std::prev(m_lru_order.end()),
                               m_lru_order.end());
        }
        m_page_cache.emplace(page_id, m_lru_order.begin());
        return p;
    }

public:
    table(const kv_store& store, std::size_t page_cache_size)
        : m_kv_store(store),
          m_page_cache_size(page_cache_size),
          m_allocated_pages(0) {}

    table(kv_store&& store, std::size_t page_cache_size)
        : m_kv_store(std::move(store)),
          m_page_cache_size(page_cache_size),
          m_allocated_pages(0) {}

    table(table&& mvfrom) noexcept
        : m_kv_store(std::move(mvfrom.m_kv_store)),
          m_page_cache_size(mvfrom.m_page_cache_size),
          m_lru_order(std::move(mvfrom.m_lru_order)),
          m_allocated_pages(mvfrom.m_allocated_pages),
          m_page_cache(std::move(mvfrom.m_page_cache)) {}

    table& operator=(table&& mvfrom) noexcept {
        m_kv_store = std::move(m_kv_store);
        m_page_cache_size = mvfrom.m_page_cache_size;
        m_lru_order = std::move(mvfrom.m_lru_order);
        m_allocated_pages = mvfrom.m_allocated_pages;
        m_page_cache = std::move(mvfrom.m_page_cache);

        return *this;
    }

    /** Access the kv_store that backs this table.
     */
    kv_store& store() {
        return m_kv_store;
    }

    /** Access the kv_store that backs this table.
     */
    const kv_store& store() const {
        return m_kv_store;
    }

    /** Read data from the page table.

        @param addr The start address of the read.
        @param buffer The output buffer to fill.
     */
    void read(std::size_t addr, utils::out_buffer& buffer) const {
        std::size_t min_page = addr / page_size();
        std::size_t max_page = (addr + buffer.size()) / page_size();

        std::size_t min_page_start = min_page * page_size();
        std::size_t min_page_offset = addr - min_page_start;
        std::size_t read_size = std::min(page_size() - min_page_offset,
                                         buffer.size());


        auto sub_buffer = buffer.substr(0, read_size);
        read_page(min_page).read(min_page_offset, sub_buffer, page_size());

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * page_size();
            std::size_t offset = max_page_start - addr;
            std::size_t read_size = addr + buffer.size() - max_page_start;
            auto sub_buffer = buffer.substr(offset, read_size);
            read_page(max_page).read(0,sub_buffer, page_size());
        }

        for (id page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * page_size();
            std::size_t offset = page_start - addr;
            auto sub_buffer = buffer.substr(offset, page_size());
            read_page(page_id).read(0, sub_buffer, page_size());
        }
    }

    /** Write data into the page table.

        @param addr The start address of the write.
        @param data The data to write into the table.
    */
    void write(std::size_t addr, const std::string_view& data) {
        std::size_t min_page = addr / page_size();
        std::size_t max_page = (addr + data.size()) / page_size();

        std::size_t min_page_start = min_page * page_size();
        std::size_t min_page_offset = addr - min_page_start;
        std::size_t write_size = std::min(page_size() - min_page_offset,
                                          data.size());
        read_page(min_page).write(min_page_offset,
                                  data.substr(0, write_size),
                                  page_size());

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * page_size();
            std::size_t write_size = addr + data.size() - max_page_start;
            std::size_t offset = max_page_start - addr;
            read_page(max_page).write(0,
                                      data.substr(offset, write_size),
                                      page_size());
        }

        for (id page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * page_size();
            std::size_t offset = page_start - addr;
            read_page(page_id).write(0,
                                     data.substr(offset, page_size()),
                                     page_size());
        }
    }

    /** Flush the internal caches back to `store()`.
     */
    void flush() {
        for (auto& [id, page] : m_lru_order) {
            if (page.dirty()) {
                m_kv_store.write(id, std::string_view(page.data(), page_size()));
                page.dirty(false);
            }
        }
        m_kv_store.flush();
    }

    /** Flush the internal caches back to `store()`.
     */
    void flush() const {
        const_cast<table*>(this)->flush();
    }

    /** Truncate the backing kv_store to the semantic end of address space.

        @param eoa The end of address space to set.
     */
    void truncate(std::size_t eoa) {
        id max_page = eoa / m_kv_store.page_size();
        m_kv_store.max_page(max_page);

        for (auto& [page_id, p] : m_lru_order) {
            if (page_id > max_page) {
                p.invalidate();
            }
        }
    }

    /** Compute the eof from the max_page of `store()`.
     */
    std::size_t eof() const {
        return m_kv_store.empty() ? 0
                                  : (m_kv_store.max_page() + 1) * page_size();
    }
};
}  // namespace h5s3::page
