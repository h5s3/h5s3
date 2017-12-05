#include <ostream>

#include "h5s3/s3.h"

int main(int, char**) {

    // Replace these with real keys to run.
    h5s3::s3::notary signer("us-east-1",
                            "AKIAIOSFODNN7EXAMPLE",
                            "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");

    auto result = h5s3::s3::get_object(signer,
                                       "h5s3-development",
                                       "/test-uploads/example.txt");
    std::cout << result << std::endl;
}
