import numpy as np
import h5s3
h5s3.register()

import h5py

f = h5py.File(
    's3://h5s3-development/testfile',
    driver='h5s3',
    aws_access_key='',
    aws_secret_key='',
    mode='r+',
)

import time
start = time.time()
# f['some_key'][:]# [:] = np.arange(500).reshape(50, 10)
# f['some_other_key'][:]# [:] = np.arange(500).reshape(10, 50)
# f['big_key'][:]#  = np.arange(3145728, dtype=float)
f['other_big_key'][5000]# = np.arange(3145728, dtype=int)
end = time.time()

print(end - start)

# assert (f['some_key'][:] == np.arange(500).reshape(50, 10)).all()
# assert (f['some_other_key'][:] == np.arange(500).reshape(10, 50)).all()
