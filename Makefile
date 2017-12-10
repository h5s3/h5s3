# Put custom environment stuff here.
-include Makefile.local

MAJOR_VERSION := 0
MINOR_VERSION := 0
MICRO_VERSION := 0

HDF5_INCLUDE_PATH ?=
HDF5_LIBRARY_PATH ?=
HDF5_LIBRARY ?= hdf5

ifneq ($(HDF5_LIBRARY_PATH),)
	EXTRA_LDFLAGS := -L$(HDF5_LIBRARY_PATH)
else
	EXTRA_LDFLAGS :=
endif

OPTLEVEL ?= 3
# This uses = instead of := so that you we can conditionally change OPTLEVEL below.
CXXFLAGS = -std=gnu++17 -Wall -Wextra -g -O$(OPTLEVEL)
LDFLAGS := $(EXTRA_LDFLAGS) -lcurl -lcrypto -lstdc++fs -l$(HDF5_LIBRARY)
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

ASAN_OPTIONS := symbolize=1
ASAN_SYMBOLIZER_PATH ?= llvm-symbolizer
ifeq ($(ADDRESS_SANITIZE), 1)
	OPTLEVEL := 0
	CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer -static-libasan
	LDFLAGS += -fsanitize=address -static-libasan
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
	echo '$(CXXFLAGS)' | cmp -s - $@ || echo '$(CXXFLAGS)' > $@

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

.PHONY: test
test: $(TESTRUNNER)
	@LD_LIBRARY_PATH=. $<

tests/%.o: tests/%.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(TEST_INCLUDE) -MD -fPIC -c $< -o $@

$(TESTRUNNER): gtest.a $(TEST_OBJECTS) $(SONAME)
	$(CXX) -o $@ $(TEST_OBJECTS) gtest.a -I $(GTEST_DIR)/include \
		-lpthread -L. -l$(LIBRARY) $(LDFLAGS)

gtest.o: $(GTEST_SRCS)
	$(CXX) $(CXXFLAGS) -I $(GTEST_DIR) -I $(GTEST_DIR)/include -c \
		$(GTEST_DIR)/src/gtest-all.cc -o $@

gtest.a: gtest.o
	$(AR) $(ARFLAGS) $@ $^

python: .compiler_flags
	cd bindings/python && \
	HDF5_INCLUDE_PATH=$(HDF5_INCLUDE_PATH) \
	HDF5_LIBRARY=$(HDF5_LIBRARY) \
	CC=$(CC) \
	CFLAGS='$(CFLAGS)' \
	CXXFLAGS='$(CXXFLAGS)' \
	LDFLAGS='$(LDFLAGS)' \
	python setup.py build_ext --inplace

.PHONY: clean
clean:
	@rm -f $(SONAME) $(SHORT_SONAME) $(OBJECTS) $(DFILES) \
		$(EXAMPLES) $(EXAMPLE_OBJECTS) $(EXAMPLE_DFILES) \
		$(TESTRUNNER) $(TEST_OBJECTS) $(TEST_DFILES) \
		gtest.o gtest.a \
		-r bindings/python/build

-include $(DFILES) $(TEST_DFILES)

print-%:
	@echo $* = $($*)
