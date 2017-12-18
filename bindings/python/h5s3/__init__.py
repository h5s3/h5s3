import h5py

from ._h5s3 import set_fapl as _set_fapl


def set_fapl(plist,
             aws_access_key,
             aws_secret_key,
             aws_region="us-east-1",
             page_size=0,
             page_cache_size=0,
             host='s3.amazonaws.com',
             use_tls=True):

    """Set the fapl for the h5s3 driver.

    Parameters
    ----------
    plist : PropFAID
        The property list.
    aws_access_key : str
        Your amazon access key id.
    aws_secret_key : str
        Your amazon secret key.
    page_size : int, optional
        The size of a data page.
    page_cache_size : int, optional
        The number of pages to cache in memory.
    host : str, optional
        The host for the aws API to ues.
    use_tls : bool, optional
        Connect to the aws API with TLS.

    Notes
    -----
    All arguments except for ``plist`` will be forwarded from
    :class:`h5py.File`.
    """
    if page_size < 0:
        raise ValueError('page_size must be >= 0: %s' % page_size)

    if page_cache_size < 0:
        raise ValueError('page_cache_size must be >= 0: %s' % page_cache_size)

    _set_fapl(
        plist.id,
        page_size,
        page_cache_size,
        aws_access_key,
        aws_secret_key,
        aws_region,
        host,
        use_tls,
    )


def register():
    """Register the h5s3 driver with h5py.

    See Also
    --------
    :func:`h5s3.unregiser`
    """
    h5py.register_driver('h5s3', set_fapl)


def unregister():
    """Unregister the h5s3 driver with h5py.

    See Also
    --------
    :func:`h5s3.register`
    """
    h5py.unregister_driver('h5s3')
