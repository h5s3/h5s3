============
Architecture
============

.. note::

   This is a developer level document. It is not necessary to understand this
   document in order to use ``h5s3``.


The ``h5s3`` code base is split into three logical layers:

1. :cpp:type:`h5s3::driver::kv_driver`: An abstract driver which accepts an
   arbitrary key-value store.

2. ``kv_store``: Different implementations of key-value stores which actually
   read and write data.

3. :cpp:type:`h5s3::page::table`: An object which translate contiguous memory
   reads and writes in paged access to a key-value store.


kv-driver
=========

The :cpp:type:`h5s3::driver::kv_driver` implements the hdf5 driver
protocol. This object is responsible for handling the C++ exception -> hdf5
exception conversions and translating raw C values into richer C++ values.

This layer can be thought of as ``h5s3``\'s membrane to hdf5.

kv-store
========

The kv-store interface defines the low-level operations for interacting with
fixed-width regions of memory. A kv-store is semantically a map from
:cpp:type:`h5s3::page::id` to a page of data. The kv-store needs to be able to
read or write an entire page at a time. The kv-store does not need to perform
IO in batches larger or smaller than one page.

The primary kv-store in ``h5s3`` is :cpp:type:`h5s3::s3_driver::s3_kv_store`
which implements the kv-store interface to talk to Amazon S3.

page table
==========

The :cpp:type:`h5s3::page::table` maps a virtual contiguous memory space into
fixed width regions called "pages". This object is responsible for translating
the posix-like API calls that hdf5 makes into a sequence of reads and writes in
a kv-store.

The :cpp:type:`h5s3::page::table` structure also implements caching to reduce
the number of trips to the underlying kv-store.
