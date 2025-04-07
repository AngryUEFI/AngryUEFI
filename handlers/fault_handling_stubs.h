#ifndef FAULT_HANDLING_STUBS_H
#define FAULT_HANDLING_STUBS_H

#include <Base.h>

#include "handlers/fault_handling.h"

extern UINT64 proto_fault_stub_start;
extern UINT64 proto_handlers_end;

// array of offsets from handler to proto_fault_stub_start
// these are handlers that write the fault number
extern UINT64 proto_fault_handlers_offsets[];
extern UINT64 proto_fault_handlers_count;

SMP_SAFE void set_idtr(void* idt_base, UINT16 idt_length);

// used as a placeholder for non-present handlers
// writes 0x100 as fault number, no matter the fault
void fallback_handler();

// called from core main loop entry point
// saves recovery information to core context
// calls actual main loop
// not really SMP_SAFE, only call during core startup
void core_main_loop_stub_wrapper(CoreContext* context);

#endif /* FAULT_HANDLING_STUBS_H */
