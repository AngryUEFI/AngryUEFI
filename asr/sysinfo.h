#ifndef SYSINFO_H
#define SYSINFO_H

#include <Base.h>

#include "asr.h"

// some system information ASRs
// namespace 0x1000

SMP_SAFE void* get_ucode_slot_addr(CoreContext* context, UINT64 slot_number); // 0x1001
SMP_SAFE void* get_machine_slot_addr(CoreContext* context, UINT64 slot_number); // 0x1002

#endif /* SYSINFO_H */
