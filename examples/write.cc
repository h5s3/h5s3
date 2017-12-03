#include "hdf5.h"

#include "h5s3/driver.h"
#include "h5s3/private/file_driver.h"

int main(int, char **) {
    H5open();

    using driver = h5s3::file_driver::file_driver;

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    if (driver::set_fapl(fapl, 64, 4)) {
        return -1;
    }

    hid_t file =  H5Fcreate("ayy.lmao",
                            H5F_ACC_TRUNC,
                            H5P_DEFAULT,
                            fapl);
    H5Pclose(fapl);

    if (file < 0) {
        return -1;
    }

    hsize_t dims[2];
    dims[0] = 4;
    dims[1] = 6;
    hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
    if (dataspace_id < 0) {
        return -1;
    }
    hid_t dataset_id = H5Dcreate2(file,
                                  "/dataset",
                                  H5T_STD_I32BE,
                                  dataspace_id,
                                  H5P_DEFAULT,
                                  H5P_DEFAULT,
                                  H5P_DEFAULT);
    if (dataset_id < 0) {
        return -1;
    }

    int n = 0;
    int dset_data[4][6];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 6; ++j) {
            dset_data[i][j] = n++;
        }
    }

    if (H5Dwrite(dataset_id,
                 H5T_NATIVE_INT,
                 H5S_ALL,
                 H5S_ALL,
                 H5P_DEFAULT,
                 dset_data) < 0) {
        return -1;
    }


    if (H5Dclose(dataset_id)) {
        return -1;
    }

    H5Fclose(file);

    H5close();
    return 0;
}
