#include <sstream>
#include <string_view>

#include "Python.h"
#include "gtest/gtest.h"

#include "minio.h"

namespace detail {
/** A `PyObject*` which calls `Py_XDECREF` when it falls out of scope.
 */
class scoped_ref final {
private:
    PyObject* m_ob;

public:
    scoped_ref(PyObject* ob) : m_ob(ob) {}

    scoped_ref(scoped_ref&& mvfrom) : m_ob(mvfrom.m_ob) {
        mvfrom.m_ob = nullptr;
    }

    scoped_ref& operator=(scoped_ref&& mvfrom) {
        std::swap(m_ob, mvfrom.m_ob);
        return *this;
    }

    ~scoped_ref() {
        Py_XDECREF(m_ob);
    }

    operator bool() const {
        return m_ob;
    }

    PyObject* get() const {
        return m_ob;
    }
};
}  // namespace detail

#if PY_MAJOR_VERSION == 2
#define PyUnicode_FromString PyString_FromString
#define PyUnicode_AsUTF8 PyString_AS_STRING

#define CAST_CODE(co) reinterpret_cast<PyCodeObject*>(co)
#else
#define CAST_CODE(co) (co)
#endif


class PythonTest : public ::testing::Test {
protected:
    static std::unique_ptr<minio> MINIO;
    static detail::scoped_ref PYTHON_NAMESPACE;

    static void SetUpTestCase() {
        MINIO = std::make_unique<minio>();

        // initialize the Python interpreter state
        Py_Initialize();

        // fill our namespace with the fixture values
        PYTHON_NAMESPACE = PyDict_New();
        detail::scoped_ref main_module = PyImport_ImportModule("__main__");
        ASSERT_TRUE(main_module) << "failed to add __main__";
        PYTHON_NAMESPACE = PyModule_GetDict(main_module.get());
        ASSERT_TRUE(PYTHON_NAMESPACE) << "failed to get module dict";

        detail::scoped_ref address = PyUnicode_FromString(MINIO->address().data());
        ASSERT_TRUE(address) << "failed to allocate address object";

        detail::scoped_ref access_key = PyUnicode_FromString(MINIO->access_key().data());
        ASSERT_TRUE(access_key) << "failed to allocate access_key object";

        detail::scoped_ref secret_key = PyUnicode_FromString(MINIO->secret_key().data());
        ASSERT_TRUE(secret_key) << "failed to allocate secret_key object";

        detail::scoped_ref region = PyUnicode_FromString(MINIO->region().data());
        ASSERT_TRUE(region) << "failed to allocate region object";

        detail::scoped_ref bucket = PyUnicode_FromString(MINIO->bucket().data());
        ASSERT_TRUE(bucket) << "failed to allocate bucket object";

        ASSERT_FALSE(
            PyDict_SetItemString(PYTHON_NAMESPACE.get(), "address", address.get()) ||
            PyDict_SetItemString(PYTHON_NAMESPACE.get(),
                                 "access_key",
                                 access_key.get()) ||
            PyDict_SetItemString(PYTHON_NAMESPACE.get(),
                                 "secret_key",
                                 secret_key.get()) ||
            PyDict_SetItemString(PYTHON_NAMESPACE.get(), "region", region.get()) ||
            PyDict_SetItemString(PYTHON_NAMESPACE.get(), "bucket", bucket.get()))
            << "failed to update __main__ module dict";
    }

    static void TearDownTestCase() {
        MINIO.reset();

        // tear down the Python interpreter state
        Py_Finalize();
    }
};

detail::scoped_ref PythonTest::PYTHON_NAMESPACE = nullptr;
std::unique_ptr<minio> PythonTest::MINIO = nullptr;

namespace detail {
/** An RAII object which clears the Python exception state when it goes out of
    scope.

    This is used to ensure that we don't bleed Python exceptions from one test
    to another.
 */
class clear_pyerr final {
public:
    ~clear_pyerr() {
        PyErr_Clear();
    }
};

std::string format_current_python_exception(PyObject* pybuffer) {
    PyErr_PrintEx(false);
    scoped_ref contents(PyObject_CallMethod(pybuffer, "getvalue", nullptr));
    if (!contents) {
        return "<unknown>";
    }
    return PyUnicode_AsUTF8(contents.get());
}
void python_test(const std::string_view& test_name,
                 std::size_t line,
                 const scoped_ref& PYTHON_NAMESPACE,
                 const std::string_view& python_source) {
    clear_pyerr clear;

    scoped_ref sys(PyImport_ImportModule("sys"));
    ASSERT_TRUE(sys) << "failed to import 'sys'";
    scoped_ref io(PyImport_ImportModule("io"));
    ASSERT_TRUE(io) << "failed to import 'io'";

    scoped_ref buf(PyObject_CallMethod(io.get(), "StringIO", nullptr));
    ASSERT_TRUE(buf) << "failed to construct StringIO";

    ASSERT_FALSE(PyObject_SetAttrString(sys.get(), "stderr", buf.get()))
        << "failed to set sys.stderr";

    std::stringstream full_source;

    // the line number reported is the *last* line of the macro, we need to
    // subtract out the newlines from the python_source
    std::size_t lines_in_source =
        std::count(python_source.begin(), python_source.end(), '\n');

    // Add a bunch of newlines so that the errors in the tests correspond to
    // the line of the files. Subtract some lines to account for the code we
    // inject around the test source.
    for (std::size_t n = 0; n < line - lines_in_source - 3; ++n) {
        full_source << '\n';
    }

    // Put the user's test in a function. We share a module dict in this test
    // suite so assignments in a test should not bleed into other tests.
    full_source << "def " << test_name << "():\n"
                << "    test_name = '" << test_name << "'\n"
                << python_source << "\n"
                << test_name << "()";

    scoped_ref code_object(
        Py_CompileString(full_source.str().data(), __FILE__, Py_file_input));
    ASSERT_TRUE(code_object.get()) << format_current_python_exception(buf.get());

    scoped_ref result(PyEval_EvalCode(CAST_CODE(code_object.get()),
                                      PYTHON_NAMESPACE.get(),
                                      PYTHON_NAMESPACE.get()));

    ASSERT_TRUE(result.get()) << format_current_python_exception(buf.get());
}
}  // namespace detail

/** Define a test case for the Python bindings.

    @param name The name of the test case.
    @param python_source The body of the test as a string of Python source code.
 */
#define PYTHON_TEST(name, python_source)                                                 \
    TEST_F(PythonTest, name) {                                                           \
        ::detail::python_test(#name, __LINE__, PYTHON_NAMESPACE, python_source);         \
    }

PYTHON_TEST(register_and_unregister, R"(
    import h5py

    import h5s3

    # h5s3 should not be in the registered drivers by default
    assert 'h5s3' not in h5py.registered_drivers(), h5py.registered_drivers()


    h5s3.register()
    assert 'h5s3' in h5py.registered_drivers(), h5py.registered_drivers()

    h5s3.unregister()
    assert 'h5s3' not in h5py.registered_drivers(), h5py.registered_drivers()
)")

PYTHON_TEST(simple_dataset, R"(
    import h5py
    import numpy as np

    import h5s3

    h5s3.register()

    file = h5py.File(
        's3://{bucket}/{test_name}'.format(bucket=bucket, test_name=test_name),
        'w',
        driver='h5s3',
        aws_access_key=access_key,
        aws_secret_key=secret_key,
        aws_region=region,
        host=address,
        use_tls=False,
    )

    # ensure that we have no data to start with
    keys = list(file.keys())
    assert not keys, keys

    data = np.arange(15).reshape(5, 3)

    # write a simple 2d array under the name 'dataset'
    file['dataset'] = data

    # the new dataset should now be in the keys set
    keys = set(file.keys())
    assert keys == {'dataset'}, keys

    # flush to ensure this goes to s3/minio
    file.flush()

    # assert that we get the same result when we read the data back
    np.testing.assert_array_equal(file['dataset'][:], data)
)")

PYTHON_TEST(simple_attribute, R"(
    import h5py

    import h5s3

    h5s3.register()

    file = h5py.File(
        's3://{bucket}/{test_name}'.format(bucket=bucket, test_name=test_name),
        'w',
        driver='h5s3',
        aws_access_key=access_key,
        aws_secret_key=secret_key,
        aws_region=region,
        host=address,
        use_tls=False,
    )

    assert dict(file.attrs) == {}, 'non-empty default attributes'
    file.attrs['attr-0'] = 'ayy'
    file.attrs['attr-1'] = 'lmao'

    assert dict(file.attrs) == {'attr-0': 'ayy', 'attr-1': 'lmao'}, (
        dict(file.attrs)
    )
)")

PYTHON_TEST(simple_group, R"(
    import h5py
    import numpy as np

    import h5s3

    h5s3.register()

    file = h5py.File(
        's3://{bucket}/{test_name}'.format(bucket=bucket, test_name=test_name),
        'w',
        driver='h5s3',
        aws_access_key=access_key,
        aws_secret_key=secret_key,
        aws_region=region,
        host=address,
        use_tls=False,
    )

    group = file.create_group('ayy')
    data = np.arange(15).reshape(5, 3)
    group['lmao'] = data

    assert set(file.keys()) == {'ayy'}, set(file.keys())
    assert set(group.keys()) == {'lmao'}, set(group.keys())

    np.testing.assert_array_equal(group['lmao'][:], data)
)")
