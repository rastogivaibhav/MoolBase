#pragma once

#ifdef _MSC_VER
#include <intrin.h>

inline int graphenedb_builtin_popcountll(unsigned long long value) {
  return static_cast<int>(__popcnt64(value));
}

#define __builtin_popcountll graphenedb_builtin_popcountll
#endif
