h5s3
=====

**Currently under development**

``h5s3`` is a driver for hdf5 1.8 and 1.10 which allows you to efficiently work
with large hdf5 files by transparently storing them in Amazon s3. ``h5s3``
allows you to only fetch the subset of a file that you need, facilitating
distributed computation against a large hdf5 file.
