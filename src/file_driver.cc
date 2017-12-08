#include "h5s3/private/file_driver.h"

namespace h5s3::file_driver {
const char* file_kv_store::name = "file_kv_store";

std::unique_ptr<char[]> file_kv_store::read(page::id page_id) const {
    auto out = std::make_unique<char[]>(m_metadata.page_size);

    std::fstream page_file(fs::path(m_path) / std::to_string(page_id),
                           page_file.in | page_file.binary);

    if (page_file) {
        page_file.read(out.get(), m_metadata.page_size);
    }

    return out;
}

void file_kv_store::write(page::id page_id, const std::string_view& data) {
    std::fstream page_file(fs::path(m_path) / std::to_string(page_id),
                           page_file.out | page_file.binary);
    if (!page_file) {
        throw std::runtime_error("failed to open page file");
    }
    page_file.write(data.data(), data.size());

    // keep track of the eof by noting the largest page we have ever seen
    std::size_t page_size = m_metadata.page_size;
    m_metadata.eof = std::max(m_metadata.eof, page_id * page_size + page_size);
}

void file_kv_store::flush() {
    std::fstream metafile(fs::path(m_path) / ".meta",
                          metafile.out | metafile.trunc);
    metafile << m_metadata;
}

void file_kv_store::truncate() {
    for (const fs::path& path : fs::directory_iterator(m_path)) {
        if (path.filename() != ".meta") {
            fs::remove(path);
        }
    }

    m_metadata.eof = 0;
    flush();
}

// declare storage for the static member m_class in this TU
template<>
H5FD_class_t file_driver::m_class{};
}  // namespace h5s3::file_driver
