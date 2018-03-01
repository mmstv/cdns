// succeeds with clang-3.9.1 -O2 -rdynamic, smaller number may be needed if code or
// compilation flags have changed
constexpr const unsigned UnsignedAfterArraySize = 8U*(1390U/8U);

constexpr const unsigned ArrayAfterUnsignedSize = 8*(19657/8);

#include "arr_t0.hxx"
