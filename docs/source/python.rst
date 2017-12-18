=================
Python (``h5py``)
=================

``h5s3`` may be used from Python with ``h5py``. By using ``h5py``, you are able
to pass S3 backed files to existing code programmed against the ``h5py``
interface.

Currently this requires installing h5py from github because the `driver
registration feature <https://github.com/h5py/h5py/pull/956>`_ has not been put
in a release yet.

Example
=======

.. code-block:: python

   import h5s3
   import h5py

   # register our driver with h5py
   h5s3.register()

   f = h5py.File(
       's3://bucket/name.h5s3',
       'r+'
       driver='h5s3',
       aws_access_key='<your-access-key>',
       aws_secret_key='<your-secret-key>',
   )

   print(f)
   # <HDF5 file "name.h5s3" (mode r+)>

   # now use f like any other hdf5 file!


API
===

.. autofunction:: h5s3.register

.. autofunction:: h5s3.unregister

.. autofunction:: h5s3.set_fapl
