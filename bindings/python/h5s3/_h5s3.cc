#include <Python.h>

#include "h5s3/private/file_driver.h"

namespace {
PyObject* set_fapl(PyObject*, PyObject* args) {
    PyObject* id_ob;
    PyObject* page_size_ob;
    PyObject* page_cache_size_ob;

    if (!PyArg_ParseTuple(args,
                          "O!O!O!:set_fapl",
                          &PyLong_Type,
                          &id_ob,
                          &PyLong_Type,
                          &page_size_ob,
                          &PyLong_Type,
                          &page_cache_size_ob)) {
        return nullptr;
    }

    hid_t id = PyLong_AsLong(id_ob);
    if (PyErr_Occurred()) {
        return nullptr;
    }

    std::size_t page_size = PyLong_AsSize_t(page_size_ob);
    if (PyErr_Occurred()) {
        return nullptr;
    }

    std::size_t page_cache_size = PyLong_AsSize_t(page_cache_size_ob);
    if (PyErr_Occurred()) {
        return nullptr;
    }

    using driver = h5s3::file_driver::file_driver;
    if (driver::set_fapl(id, page_size, page_cache_size)) {
        PyErr_SetString(PyExc_ValueError, "failed to set the driver");
        return nullptr;
    }

    Py_RETURN_NONE;
}

PyMethodDef module_methods[] = {
    {"set_fapl", (PyCFunction) set_fapl, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "h5s3._h5s3",
    nullptr,
    -1,
    module_methods,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

PyMODINIT_FUNC
PyInit__h5s3(void)
{
    return PyModule_Create(&module);
}
}  // namespace
