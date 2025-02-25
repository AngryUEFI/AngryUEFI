#ifndef STUBS_H
#define STUBS_H

#include <Base.h>

UINT64 test_stub(UINT64 a, UINT64 b);
UINT64 read_msr_stub(UINT32 target_msr);
UINT64 apply_ucode_simple(UINT8* ucode);

#endif /* PROSTUBS_HOCOL_H */
