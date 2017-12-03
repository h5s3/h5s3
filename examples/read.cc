#include <iostream>

#include "hdf5.h"

#include "h5s3/private/file_driver.h"

int main(int, char **) {
    H5open();

    using driver = h5s3::file_driver::file_driver;

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    driver::set_fapl(fapl, 0, 4);
    hid_t file =  H5Fopen("ayy.lmao", H5F_ACC_RDWR, fapl);
    H5Pclose(fapl);

    if (file < 0) {
        return -1;
    }

    int dset_data[4][6] = {0};
    hid_t dataset_id = H5Dopen2(file, "/dataset", H5P_DEFAULT);

    H5Dread(dataset_id,
            H5T_NATIVE_INT,
            H5S_ALL,
            H5S_ALL,
            H5P_DEFAULT,
            dset_data);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 6; ++j) {
            std::cout << '(' << i << ", " << j << "): "
                      << dset_data[i][j] << '\n';
        }
    }


    H5Dclose(dataset_id);
    H5Fclose(file);

    H5close();
    return 0;
}
