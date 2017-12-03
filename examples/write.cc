#include "hdf5.h"

#include "h5s3/driver.h"

int main(int, char **) {
    H5open();

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    h5s3::driver::h5s3_set_fapl(fapl);
    hid_t file =  H5Fcreate("ayy.lmao", H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    H5Pclose(fapl);

    hsize_t dims[2];
    dims[0] = 4;
    dims[1] = 6;
    hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
    hid_t dataset_id = H5Dcreate2(file,
                                  "/dataset",
                                  H5T_STD_I32BE,
                                  dataspace_id,
                                  H5P_DEFAULT,
                                  H5P_DEFAULT,
                                  H5P_DEFAULT);

    int n = 0;
    int dset_data[4][6];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 6; ++j) {
            dset_data[i][j] = n++;
        }
    }

    H5Dwrite(dataset_id,
             H5T_NATIVE_INT,
             H5S_ALL,
             H5S_ALL,
             H5P_DEFAULT,
             dset_data);


    H5Dclose(dataset_id);
    H5Fclose(file);

    return 0;
}
