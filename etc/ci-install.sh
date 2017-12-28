# I hate yaml-as-code so damn much. Just use a bash script
set +e

if [ -n "$GCC" ];then
    export CXX="g++-$GCC" CC="gcc-$GCC"
elif [ -n "$CLANG" ];then
    export CXX="clang++" CC="clang"
fi

${PYTHON} -m venv venv
source venv/bin/activate
${PYTHON} -m pip install git+https://github.com/h5s3/h5py.git
${PYTHON} -m pip install -e bindings/python
