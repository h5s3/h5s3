``h5s3`` for Python
===================

``h5s3`` integration for ``h5py``.

Usage
-----

.. code-block:: python

   import h5s3
   import h5py

   h5s3.register()

   f = h5py.File('s3://bucket/object', driver='h5s3')
   # use f like any other hdf5 file!


This currently depends on a fork of h5py to support custom drivers. We are
working on upstreaming this change.
