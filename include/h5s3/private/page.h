#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

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

    kv_store m_kv_store;
    const std::size_t m_page_cache_size;
    using list_type = std::list<std::tuple<id, page>>;
    mutable list_type m_lru_order;
    mutable std::unordered_map<id, typename list_type::iterator> m_page_cache;

    class page {
    private:
        bool m_dirty;
        std::unique_ptr<char[]> m_data;

    public:
        page(std::unique_ptr<char[]>&& data)
            : m_dirty(false),
              m_data(std::move(data)) {}

        page(page&& mvfrom) noexcept
            : m_dirty(mvfrom.m_dirty),
              m_data(std::move(mvfrom.m_data)) {}

        page& operator=(page&& mvfrom) noexcept {
            m_dirty = mvfrom.m_dirty;
            m_data = std::move(mvfrom.m_data);
            return *this;
        }

        void read(std::size_t addr, std::string_view& buffer) const {
            std::memcpy(buffer.data(), &m_data[addr], buffer.size());
        }

        void write(std::size_t addr, const std::string_view& data) {
            std::memcpy(&m_data[addr], data.data(), data.size());

            // mark that this page is dirty
            m_dirty = true;
        }

        const char* data() const {
            return m_data.get();
        }

        bool dirty() {
            return m_dirty;
        }

        void dirty(bool dirty) {
            m_dirty = dirty;
        }

        bool dirty() const {
            return m_dirty;
        }
    };

    std::size_t page_size() const {
        return m_kv_store.metadata.page_size;
    }

    std::size_t page_size() {
        return const_cast<table*>(this)->page_size();
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
            const auto& [to_evict, page] = m_lru_order.back();
            if (page.dirty()) {
                m_kv_store.write(to_evict,
                                 std::string_view(page.data(), page_size()));
            }
            m_page_cache.erase(to_evict);
            m_lru_order.pop_back();

        }

        auto& [_, p] = m_lru_order.emplace_front(page_id,
                                                 m_kv_store.read(page_id));
        m_page_cache.emplace(page_id, m_lru_order.begin());
        return p;
    }

public:
    table(const kv_store& store, std::size_t page_cache_size)
        : m_kv_store(store), m_page_cache_size(page_cache_size) {}

    table(kv_store&& store, std::size_t page_cache_size)
        : m_kv_store(std::move(store)), m_page_cache_size(page_cache_size) {}

    table(table&& mvfrom) noexcept
        : m_kv_store(std::move(m_kv_store)),
          m_page_cache_size(mvfrom.m_page_cache_size),
          m_lru_order(std::move(mvfrom.m_lru_order)),
          m_page_cache(std::move(mvfrom.m_page_cache)) {}

    table& operator=(table&& mvfrom) noexcept {
        m_kv_store = std::move(m_kv_store);
        m_page_cache_size = mvfrom.m_page_cache_size;
        m_lru_order = std::move(mvfrom.m_lru_order);
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
    void read(std::size_t addr, std::string_view& buffer) const {
        std::size_t min_page = addr / page_size();
        std::size_t max_page = (addr + buffer.size()) / page_size();

        std::size_t min_page_start = min_page * page_size();
        std::size_t min_page_offset = addr - min_page_start;
        std::size_t read_size = std::min(page_size() - min_page_offset,
                                         buffer.size());
        read_page(min_page).read(min_page_offset, buffer.substr(0, read_size));

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * page_size();
            std::size_t offset = max_page_start - addr;
            std::size_t read_size = addr + buffer.size() - max_page_start;
            read_page(max_page).read(0, buffer.substr(offset, read_size));
        }

        for (id page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * page_size();
            std::size_t offset = page_start - addr;
            read_page(page_id).read(0, buffer.substr(offset, page_size()));
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
        read_page(min_page).write(min_page_offset, data.substr(0, write_size));

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * page_size();
            std::size_t write_size = addr + data.size() - max_page_start;
            std::size_t offset = max_page_start - addr;
            read_page(max_page).write(0, data.substr(offset, write_size));
        }

        for (id page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * page_size();
            std::size_t offset = page_start - addr;
            read_page(page_id).write(0, data.substr(offset, page_size()));
        }
    }

    /** Flush the internal caches back to `store()`.
     */
    void flush() {
        for (auto& [id, page] : m_lru_order) {
            m_write_page(id, std::string_view(page.data(), page_size()));
            page.dirty(false);
        }
    }

    /** Flush the internal caches back to `store()`.
     */
    void flush() const {
        const_cast<table*>(this)->flush();
    }
};
}  // namespace h5s3::page
