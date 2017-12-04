#include "h5s3/driver.h"

namespace h5s3::driver {
std::istream& operator>>(std::istream& stream, metadata& m) {
    stream >> m.page_size;

    char newline;
    stream.read(&newline, 1);
    if (newline != '\n') {
        throw std::runtime_error("malformed metadata file");
    }

    stream >> m.eof;

    return stream;
}

std::ostream& operator<<(std::ostream& stream, metadata& m) {
    return stream << m.page_size << '\n' << m.eof << '\n';
}
}  // namespace h5s3::driver
