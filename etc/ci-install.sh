# I hate yaml-as-code so damn much. Just use a bash script
set +e

if [ -n "$GCC" ];then
    export CXX="g++-$GCC" CC="gcc-$GCC"
elif [ -n "$CLANG" ];then
    export CXX="clang++" CC="clang"
fi

pip install h5py
HDF5_INCLUDE_PATH=/usr/include/hdf5/serial/ \
    HDF5_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/hdf5/serial \
    pip install -e bindings/python
