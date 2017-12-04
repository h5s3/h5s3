#!/usr/bin/env python
import glob
import os
from setuptools import setup, find_packages, Extension
import sys

long_description = ''

if 'upload' in sys.argv:
    with open('README.rst') as f:
        long_description = f.read()


extension = Extension(
    'h5s3._h5s3',
    sources=['h5s3/_h5s3.cc'] + glob.glob('cxx/src/*.cc'),
    libraries=[
        'curl',
        'crypto',
        'stdc++fs',
        os.environ.get('HDF_LIBRARY', 'hdf5'),
    ],
    language='c++',
    include_dirs=['cxx/include'],
    extra_compile_args=[
        '-Wall',
        '-Wextra',
        '-std=gnu++17',
    ],
)


setup(
    name='h5s3',
    version='0.1.0',
    description='h5s3 bindings for Python',
    author='Joe Jevnik and Scott Sanderson',
    author_email='joejev@gmail.com',
    license='Apache 2.0',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Science/Research;',
        'License :: OSI Approved :: Apache Software License',
        'Natural Language :: English',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: Implementation :: CPython',
        'Operating System :: OS Independent',
        'Topic :: System :: Distributed Computing',
        'Topic :: Scientific/Engineering :: Interface Engine/Protocol Translator'  # noqa
    ],
    url="https://github.com/h5s3/h5s3",
    packages=find_packages(),
    long_description=long_description,
    ext_modules=[extension],
    install_requires=['h5py'],
)