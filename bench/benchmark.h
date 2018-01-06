#pragma once

#include "benchmark/benchmark.h"

#ifdef __GNUC__
#define UNUSED(a) a __attribute__((unused))
#else
#define UNUSED(a) a
#endif

#define INNER_CAT(a, b) a ## b
#define CAT(a, b) INNER_CAT(a, b)

#define ITERATE(state) for (auto UNUSED(CAT(_unused, __LINE__)) : state)
