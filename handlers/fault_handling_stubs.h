#ifndef FAULT_HANDLING_STUBS_H
#define FAULT_HANDLING_STUBS_H

#include <Base.h>

extern UINT64 proto_fault_stub_start;
extern UINT64 proto_handlers_end;

// array of offsets from handler to proto_fault_stub_start
extern UINT64 proto_fault_handlers_offsets[];
extern UINT64 proto_fault_handlers_count;

#endif /* FAULT_HANDLING_STUBS_H */
