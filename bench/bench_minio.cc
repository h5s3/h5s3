#include "benchmark.h"

#include "h5s3/minio.h"

class minio_fixture : public benchmark::Fixture {
protected:
    static h5s3::utils::minio m_minio;
};

h5s3::utils::minio minio_fixture::m_minio;

BENCHMARK_F(minio_fixture, create_new_file)(benchmark::State& state) {
    BENCHMARK_LOOP(state) {
        // fill this in when hdfgroup.org is back online
    }
}
