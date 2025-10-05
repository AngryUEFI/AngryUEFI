#ifndef DSM_H
#define DSM_H

#include <Base.h>

#include "asr.h"

#define MAXIMUM_DSM_ENTRIES 128

// prepare DSM internal state
// should be called at start of ASM stub
SMP_SAFE void* prepare_dsm(CoreContext* context);
// prepare DSM for capture
// should be called closely before starting the capture
SMP_SAFE void* prepare_dsm_capture(CoreContext* context);
// store events to DSM result buffer
// should be called after stopping the cature
SMP_SAFE void* store_dsm_capture(CoreContext* context);
// return the absolute address of the stub to start/stop the DSM trace
SMP_SAFE void* get_start_stop_stub_asm_address(CoreContext* context);
SMP_SAFE void* copy_dsm_entries(CoreContext* context, UINT8* target_buffer, UINT64 start_index, UINT64 entry_count);

// Recommended to use ASM directly to reduce amount of spurious events in buffer
// ASRs provided as reference
// start capturing events
SMP_SAFE void* start_dsm_capture(CoreContext* context);
// stop capturing events
SMP_SAFE void* stop_dsm_capture(CoreContext* context);

SMP_SAFE void enable_dsm_in_cr4_stub();

// ucode read/write primitive
// essentially call into ucode to access internal memory
// requires instructions to be ucode hooked
UINT64 write_fenced_fptan(UINT64 arg1, UINT64 arg2, UINT64 flags);
UINT64 read_fenced_fpatan(UINT64 arg1, UINT64 arg2, UINT64 flags);
UINT64 write_fenced_fprem(UINT64 arg1, UINT64 arg2, UINT64 flags);
// uses a different calling convention for less executed instructions
// only use from asm!
UINT64 write_fenced_fprem_asm(UINT64 arg1, UINT64 arg2, UINT64 flags);

// must be called once from the boot core to init the context for DSM support
// allocates memory etc. that can not be done on an AP
void init_dsm_for_context(CoreContext* context);

// sets up core specific configs that must be done on the AP
// called during startup of the AP
// must be re-entrant to support recovery
SMP_SAFE void init_dsm_on_core(CoreContext* contex);

#pragma pack(push,1)
typedef struct DSMEvent_s {
    UINT64 data0;
    UINT64 data1;
} DSMEvent;

typedef struct DSM_file_header_s {
    UINT64 version_info;
    UINT64 mask;
    UINT64 sel;
    UINT64 types;
    UINT64 idx_info;
    UINT64 num_items;
} DSM_file_header;

#pragma pack(pop)

// control structure for DSM
typedef struct DSMControl_s {
    DSM_file_header file_header;
    DSMEvent* event_buffer;
    UINT64 orig;
} DSMControl;

#endif /* DSM_H */
