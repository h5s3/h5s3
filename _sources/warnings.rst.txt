========================
Warnings and Limitations
========================

Experimental Software
=====================

``h5s3`` is currently **not** robust software. Assume it does **not** work
correctly. This package is in a pre-release phase so that people may begin
testing and providing feedback.

Concurrent Access
=================

``h5s3`` does **not** turn hdf5 into a database. Amazon S3 is only eventually
consistent and does not provide atomic access to objects nor locking
mechanisms.

For safe, deterministic access, you may either have:

- **Exactly** one read-write user at a time.
- Unlimited **read-only** users.

While a user is writing to a file, changes will not be sent to other
clients. Other clients will still see stale data for a potentially unlimited
amount of time. If two clients are writing to a file at the same time, their
writes will not be sent to each other, so stale data may overwrite new data when
flushing the client's in-memory pages to S3.  For these reasons, we recommend
sticking to one of the two usage patterns described above.
