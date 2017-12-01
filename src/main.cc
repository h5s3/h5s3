#include <iostream>
#include "h5s3/private/curl.h"
#include "h5s3/private/page.h"

int main(int, char **) {
    constexpr std::size_t page_size = 10;
    std::unordered_map<std::size_t, std::string> map;

    auto write_page = [&](std::size_t id, const std::string_view& data) {
        map[id] = data;
    };
    auto read_page = [&](std::size_t id) {
        std::string& data = map[id];
        if (!data.size()) {
            data.resize(page_size, '-');
        }

        auto ptr = std::make_unique<char[]>(page_size);
        std::memcpy(ptr.get(), data.data(), page_size);
        return ptr;
    };

    h5s3::page::table table(read_page, write_page, page_size, 4);

    table.write(5, "abc");
    table.write(15, "def");

    std::string buffer;
    buffer.resize(30);
    table.read_into(2, 30, buffer.data());

    std::cout << buffer << '\n';
    return 0;
}
