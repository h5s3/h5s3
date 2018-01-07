# Put custom environment stuff here.
-include Makefile.local

PYTHON ?= python3

MAJOR_VERSION := 0
MINOR_VERSION := 0
MICRO_VERSION := 0

HDF5_INCLUDE_PATH ?=
HDF5_LIBRARY_PATH ?=
HDF5_LIBRARY ?= hdf5

CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format
GTEST_BREAK ?= 1

OPTLEVEL ?= 3
# This uses = instead of := so that you we can conditionally change OPTLEVEL below.
CXXFLAGS = -std=gnu++17 -Wall -Wextra -g -O$(OPTLEVEL)
LDFLAGS := -lcurl -lcrypto -lstdc++fs -l$(HDF5_LIBRARY)
INCLUDE_DIRS := include/ $(HDF5_INCLUDE_PATH)
INCLUDE := $(foreach d,$(INCLUDE_DIRS), -I$d)
LIBRARY := h5s3
SHORT_SONAME := lib$(LIBRARY).so
SONAME := $(SHORT_SONAME).$(MAJOR_VERSION).$(MINOR_VERSION).$(MICRO_VERSION)

OS := $(shell uname)
ifeq ($(OS),Darwin)
	SONAME_FLAG := install_name
	AR := libtool
	ARFLAGS := -static -o
else
	SONAME_FLAG := soname
endif

ifneq ($(HDF5_LIBRARY_PATH),)
	LDFLAGS += -L$(HDF5_LIBRARY_PATH)
endif

# Sanitizers
ASAN_OPTIONS := symbolize=1
LSAN_OPTIONS := suppressions=testleaks.supp
ASAN_SYMBOLIZER_PATH ?= llvm-symbolizer

SANITIZE_ADDRESS ?= 0
ifneq ($(SANITIZE_ADDRESS),0)
	OPTLEVEL := 0
	CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer -static-libasan
	LDFLAGS += -fsanitize=address -static-libasan
endif

SANITIZE_UNDEFINED ?= 0
ifneq ($(SANITIZE_UNDEFINED),0)
	OPTLEVEL := 0
	CXXFLAGS += -fsanitize=undefined
	LDFLAGS += -lubsan
endif

# Coverage
GCOV := gcov-7
# Set this to 1 if you want to build with gcov support
COVERAGE ?= 0
ifneq ($(COVERAGE),0)
	CXXFLAGS += -fprofile-arcs -ftest-coverage
	LDFLAGS += -fprofile-arcs -lgcov
endif

SOURCES := $(wildcard src/*.cc)
OBJECTS := $(SOURCES:.cc=.o)
DFILES :=  $(SOURCES:.cc=.d)

EXAMPLE_SOURCES := $(wildcard examples/*.cc)
EXAMPLE_OBJECTS := $(EXAMPLE_SOURCES:.cc=.o)
EXAMPLE_DFILES :=  $(EXAMPLE_SOURCES:.cc=.d)
EXAMPLES := $(EXAMPLE_SOURCES:.cc=)

GTEST_ROOT:= submodules/googletest
GTEST_DIR := $(GTEST_ROOT)/googletest
GTEST_HEADERS := $(wildcard $(GTEST_DIR)/include/gtest/*.h) \
	$(wildcard $(GTEST_DIR)/include/gtest/internal/*.h)
GTEST_SRCS := $(wildcard $(GTEST_DIR)/src/*.cc) \
	$(wildcard $(GTEST_DIR)/src/*.h) $(GTEST_HEADERS)

TEST_SOURCES := $(wildcard tests/*.cc)
TEST_DFILES := $(TEST_SOURCES:.cc=.d)
TEST_OBJECTS := $(TEST_SOURCES:.cc=.o)
TEST_HEADERS := $(wildcard tests/*.h)  $(GTEST_HEADERS)
TEST_INCLUDE := -I tests -I $(GTEST_DIR)/include
TESTRUNNER := tests/run

ALL_SOURCES := $(SOURCES) $(EXAMPLE_SOURCES) $(TEST_SOURCES)
ALL_HEADERS := include/h5s3/**.h

PYTHON_SONAME := _h5s3$(shell $(PYTHON)-config --extension-suffix)
PYTHON_EXTENSION := bindings/python/h5s3/$(PYTHON_SONAME)

ALL_FLAGS := 'CFLAGS=$(CFLAGS) CXXFLAGS=$(CXXFLAGS) LDFLAGS=$(LDFLAGS)'

.PHONY: all
all: $(SONAME)

.PHONY: local-install
local-install: $(SONAME)
	cp $< ~/lib
	@rm -f ~/lib/$(SHORT_SONAME)
	ln -s ~/lib/$(SONAME) ~/lib/$(SHORT_SONAME)
	cp -rf include/$(LIBRARY) ~/include

# Write our current compiler flags so that we rebuild if they change.
force:
.compiler_flags: force
	@echo '$(ALL_FLAGS)' | cmp -s - $@ || echo '$(ALL_FLAGS)' > $@

$(SONAME): $(OBJECTS) $(HEADERS)
	$(CXX) $(OBJECTS) -shared -Wl,-$(SONAME_FLAG),$(SONAME) \
		-o $(SONAME) $(LDFLAGS)
	@rm -f $(SHORT_SONAME)
	ln -s $(SONAME) $(SHORT_SONAME)

src/%.o: src/%.cc .compiler_flags
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MD -fPIC -c $< -o $@

examples/%.o: examples/%.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MD -fPIC -c $< -o $@

examples/%: examples/%.o $(SONAME)
	$(CXX) -o $@ $< -L. -l$(LIBRARY) $(LDFLAGS)

.PHONY: example-%
example-%: examples/%
	LD_LIBRARY_PATH=. $<

testbin/minio:
	mkdir testbin || true
	curl -L https://dl.minio.io/server/minio/release/linux-amd64/minio > $@
	chmod +x $@

testbin/mc:
	mkdir testbin || true
	curl -L https://dl.minio.io/client/mc/release/linux-amd64/mc > $@
	chmod +x $@

.PHONY: test
test: $(TESTRUNNER) testbin/minio testbin/mc
	@LD_LIBRARY_PATH=. LSAN_OPTIONS=$(LSAN_OPTIONS) $<

.PHONY: gdbtest
gdbtest: $(TESTRUNNER)
	@LD_LIBRARY_PATH=. GTEST_BREAK_ON_FAILURE=$(GTEST_BREAK) gdb -ex run $<

tests/test_python.o: tests/test_python.cc .compiler_flags $(PYTHON_EXTENSION)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(TEST_INCLUDE) -MD -fPIC -c $< -o $@ \
		$(shell $(PYTHON)-config --includes)

tests/%.o: tests/%.cc .compiler_flags
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(TEST_INCLUDE) -MD -fPIC -c $< -o $@

$(TESTRUNNER): gtest.a $(TEST_OBJECTS) $(SONAME)
	$(CXX) -o $@ $(TEST_OBJECTS) gtest.a -I $(GTEST_DIR)/include \
		-lpthread -L. -l$(LIBRARY) $(LDFLAGS) \
		$(shell $(PYTHON)-config --ldflags)

gtest.o: $(GTEST_SRCS) .compiler_flags
	$(CXX) $(CXXFLAGS) -I $(GTEST_DIR) -I $(GTEST_DIR)/include -c \
		$(GTEST_DIR)/src/gtest-all.cc -o $@

gtest.a: gtest.o
	$(AR) $(ARFLAGS) $@ $^

$(PYTHON_EXTENSION): .compiler_flags bindings/python/h5s3/_h5s3.cc
	cd bindings/python && \
	HDF5_INCLUDE_PATH=$(HDF5_INCLUDE_PATH) \
	HDF5_LIBRARY=$(HDF5_LIBRARY) \
	CC=$(CC) \
	CXX=$(CXX) \
	CFLAGS='$(CFLAGS)' \
	CXXFLAGS='$(CXXFLAGS)' \
	LDFLAGS='$(LDFLAGS)' \
	$(PYTHON) setup.py build_ext --inplace

.PHONY: tidy
tidy:
	$(CLANG_TIDY) $(ALL_SOURCES) $(ALL_HEADERS) --header-filter=include/ \
		-checks=-*,clang-analyzer-*,clang-analyzer-* \
		-- -x c++ --std=gnu++17 \
		$(INCLUDE) $(TEST_INCLUDE) $(shell $(PYTHON)-config --includes)

.PHONY: format
format:
	$(CLANG_FORMAT) -i $(ALL_SOURCES) $(ALL_HEADERS)

.PHONY: coverage
coverage:
	@COVERAGE=1 $(MAKE) __real-coverage


.PHONY: __real-coverage
__real-coverage: test
	@GCOV=$(GCOV) ./etc/coverage-report src/ include/h5s3/ tests/

.PHONY: clean
clean:
	@rm -f $(SONAME) $(SHORT_SONAME) $(OBJECTS) $(DFILES) \
		$(EXAMPLES) $(EXAMPLE_OBJECTS) $(EXAMPLE_DFILES) \
		$(TESTRUNNER) $(TEST_OBJECTS) $(TEST_DFILES) \
		gtest.o gtest.a \
		$(PYTHON_EXTENSION) \
		-r bindings/python/build

-include $(DFILES) $(TEST_DFILES)

print-%:
	@echo $* = $($*)
