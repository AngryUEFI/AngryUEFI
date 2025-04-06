#include "fault_handling.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "handlers/cores.h"
#include "handlers/fault_handling_stubs.h"

void init_fault_handlers_on_core(CoreContext* context) {
    // we want an alignment piece of memory
    // usually it should be aligned already, but we need to be sure
    const UINTN alignment = 16;
    CoreFaultInfo* orig_fault_info = AllocateZeroPool(sizeof(CoreFaultInfo) + alignment);
    CoreFaultInfo* fault_info = orig_fault_info;

    UINTN aligment_diff = ((UINT64)orig_fault_info) % alignment;
    if (aligment_diff != 0) {
        fault_info = (CoreFaultInfo*)(((UINT8*)orig_fault_info) + (alignment - aligment_diff));
    }

    context->fault_info = fault_info;
    context->original_fault_info_ptr = orig_fault_info;

    
    // already 16 byte aligned due to .align in stubs
    const UINT64 fault_handlers_length = (UINT64)&proto_handlers_end - (UINT64)&proto_fault_stub_start;
    UINT8* orig_fault_handlers = AllocatePool(fault_handlers_length + alignment);
    UINT8* fault_handlers = orig_fault_handlers;
    context->original_fault_handlers_ptr = fault_handlers;
    
    aligment_diff = ((UINT64)orig_fault_handlers) % alignment;
    if (aligment_diff != 0) {
        fault_handlers = orig_fault_handlers + (alignment - aligment_diff);
    }
    
    CopyMem(fault_handlers, &proto_fault_stub_start, fault_handlers_length);
    // first 8 Bytes are the core context pointer
    *(UINT64*)fault_handlers = (UINT64)context;

    context->core_idt_descriptor = AllocateZeroPool(sizeof(IDT_DESCRIPTOR));
}
