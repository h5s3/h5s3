``h5s3`` for Python
===================

``h5s3`` integration for ``h5py``.

Usage
-----

.. code-block:: python

   import h5s3
   import h5py

   # register our driver with h5py
   h5s3.register()

   f = h5py.File(
       's3://bucket/object',
       driver='h5s3',
       aws_access_key='<your-access-key>',
       aws_secret_key='<your-secret-key>',
   )

   # now use f like any other hdf5 file!


Currently this requires installing h5py from github because the `driver
registration feature <https://github.com/h5py/h5py/pull/956>`_ has not been put
in a release yet.
