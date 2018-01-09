#include "benchmark.h"

#include "h5s3/minio.h"
#include "h5s3/private/h5_wrapper.h"
#include "h5s3/private/literals.h"
#include "h5s3/private/tmpdir.h"
#include "h5s3/private/s3_driver.h"

using driver = h5s3::s3_driver::s3_driver;
using h5s3::utils::operator""_KB;
using h5s3::utils::operator""_array;

class minio_fixture : public benchmark::Fixture {
protected:
    static h5s3::utils::minio MINIO;
};

h5s3::utils::minio minio_fixture::MINIO;

namespace {
void page_size_args(benchmark::internal::Benchmark* b) {
    for (int n = 0; n < 10; ++n) {
        b->Arg(4_KB << n);
    }
}
}  // namespace

BENCHMARK_DEFINE_F(minio_fixture, create_new_file)(benchmark::State& state) {
    auto options = "abcdefghijklmnopqrstuvwxyz0123456789"_array;

    h5s3::h5::context ctx;
    h5s3::h5::fapl f;
    driver::set_fapl(f.get(),
                     state.range(0),
                     0,
                     MINIO.access_key().data(),
                     MINIO.secret_key().data(),
                     MINIO.region().data(),
                     MINIO.address().data(),
                     false);

    BENCHMARK_LOOP(state) {
        state.PauseTiming();
        std::stringstream s;
        s << "s3://" << MINIO.bucket() << '/';
        for (int n = 0; n < 10; ++n) {
            s << options[std::rand() % options.size()];
        }
        std::string uri = s.str();
        state.ResumeTiming();
        auto file = h5s3::h5::file::create(uri, H5F_ACC_TRUNC, H5P_DEFAULT, f.get());
        benchmark::DoNotOptimize(file);
    }
}

BENCHMARK_REGISTER_F(minio_fixture, create_new_file)->Apply(page_size_args);
