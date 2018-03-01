// fails with clang-3.9.1 -O2 on Linux, smaller number may be sufficient if -rdynamic
// is used
constexpr const unsigned UnsignedAfterArraySize = 8*(3800/8);

constexpr const unsigned ArrayAfterUnsignedSize = 8*(19657/8);

#include "arr_t0.hxx"
