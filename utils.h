#ifndef UTILS_H
#define UTILS_H

#include <Base.h>

UINT64 get_tsc();
// return tsc2 - tsc1 in ms
UINT64 get_ms_diff(UINT64 tsc1, UINT64 tsc2);

#endif /* UTILS_H */
