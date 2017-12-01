#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace h5s3 {
namespace page {

template<typename read_page_callback_type, typename write_page_callback_type>
class table {
private:
    class page;

    const std::size_t m_page_size;

    read_page_callback_type m_read_page;
    write_page_callback_type m_write_page;

    const std::size_t m_page_cache_size;

    using list_type = std::list<std::tuple<std::size_t, page>>;
    mutable list_type m_lru_order;
    mutable std::unordered_map<std::size_t,
                               typename list_type::iterator> m_page_cache;

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

        void read_into(std::size_t addr,
                       std::size_t size,
                       char* buffer) const {
            std::memcpy(buffer, &m_data[addr], size);
        }

        void write(std::size_t addr, std::string_view data) {
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

    page& read_page(std::size_t page_id) const {
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
                m_write_page(to_evict,
                             std::string_view(page.data(), m_page_size));
            }
            m_page_cache.erase(to_evict);
            m_lru_order.pop_back();

        }

        auto& [_, p] = m_lru_order.emplace_front(page_id, m_read_page(page_id));
        m_page_cache.emplace(page_id, m_lru_order.begin());
        return p;
    }

public:
    table(read_page_callback_type read_page,
          write_page_callback_type write_page,
          std::size_t page_size,
          std::size_t page_cache_size)

        : m_page_size(page_size),
          m_read_page(read_page),
          m_write_page(write_page),
          m_page_cache_size(page_cache_size) {}

    void read_into(std::size_t addr,
                   std::size_t size,
                   char* buffer) const {
        std::size_t min_page = addr / m_page_size;
        std::size_t max_page = (addr + size) / m_page_size;

        std::size_t min_page_start = min_page * m_page_size;
        std::size_t min_page_offset = addr - min_page_start;
        std::size_t read_size = std::min(m_page_size - min_page_offset, size);
        read_page(min_page).read_into(min_page_offset, read_size, buffer);

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * m_page_size;
            std::size_t offset = max_page_start - addr;
            std::size_t read_size = addr + size - max_page_start;
            read_page(max_page).read_into(0, read_size, buffer + offset);
        }

        for (std::size_t page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * m_page_size;
            std::size_t offset = page_start - addr;
            read_page(page_id).read_into(0, m_page_size, buffer + offset);
        }
    }

    void write(std::size_t addr, std::string_view data) {
        std::size_t min_page = addr / m_page_size;
        std::size_t max_page = (addr + data.size()) / m_page_size;

        std::size_t min_page_start = min_page * m_page_size;
        std::size_t min_page_offset = addr - min_page_start;
        std::size_t write_size = std::min(m_page_size - min_page_offset,
                                          data.size());
        read_page(min_page).write(min_page_offset, data.substr(0, write_size));

        if (min_page != max_page) {
            std::size_t max_page_start = max_page * m_page_size;
            std::size_t write_size = addr + data.size() - max_page_start;
            std::size_t offset = max_page_start - addr;
            read_page(max_page).write(0, data.substr(offset, write_size));
        }

        for (std::size_t page_id = min_page + 1;
             page_id < max_page;
             ++page_id) {
            std::size_t page_start = page_id * m_page_size;
            std::size_t offset = page_start - addr;
            read_page(page_id).write(0, data.substr(offset, m_page_size));
        }
    }

    void flush() {
        for (auto& [id, page] : m_lru_order) {
            m_write_page(id, std::string_view(page.data(), m_page_size));
            page.dirty(false);
        }
    }

    void flush() const {
        const_cast<table*>(this)->flush();
    }
};
}  // namespace page
}  // namespace h5s3
