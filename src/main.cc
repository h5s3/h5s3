#include <iostream>
#include "h5s3/private/curl.h"

int main(int, char **) {
    h5s3::curl::session session;
    std::cout << session.get("http://example.com") << std::endl;
    return 0;
}
