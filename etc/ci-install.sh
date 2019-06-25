# I hate yaml-as-code so damn much. Just use a bash script
set +e

if [ -n "$GCC" ];then
    export CXX="g++-$GCC" CC="gcc-$GCC"
elif [ -n "$CLANG" ];then
    export CXX="clang++" CC="clang"
fi

${PYTHON} -m pip install --user h5py
${PYTHON} -m pip install --user -e bindings/python
