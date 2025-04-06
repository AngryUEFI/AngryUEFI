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

    // memory location with IDT descriptor
    // loaded to core via LIDT later
    context->core_idt_descriptor = AllocateZeroPool(sizeof(IDT_DESCRIPTOR));

    // allocate space for an IDT for this core
    IDT_ENTRY* idt = AllocateZeroPool(sizeof(IDT_ENTRY) * proto_fault_handlers_count);

    // set IDT descriptor members
    context->core_idt_descriptor->Base = (UINT64)idt;
    // limit is the amount of handlers, minus 1 due to x86 being x86
    context->core_idt_descriptor->Limit = proto_fault_handlers_count - 1;

    // fill the IDT with our handlers
    for (UINTN i = 0; i < proto_fault_handlers_count; i++) {
        // offset from start of handler buffer
        UINT64 handler_offset = proto_fault_handlers_offsets[i];
        // absolute address for this core
        UINT64 handler_address = (UINT64)fault_handlers + handler_offset;

        IDT_ENTRY* current_descriptor = &idt[i];

        current_descriptor->OffsetLow = (UINT16)(handler_address & 0xFFFF);
        current_descriptor->OffsetMid = (UINT16)((handler_address >> 16) & 0xFFFF);
        current_descriptor->OffsetHigh = (UINT32)((handler_address >> 32) & 0xFFFFFFFF);

        current_descriptor->SegmentSelector = CODE_SEGMENT;
        current_descriptor->Ist = 0; // use current stack
        current_descriptor->Attributes = 0x8E; // Interrupt Gate: 0x8E (p=1, dpl=0b00, type=0b1110 => type_attributes=0b1000_1110=0x8E)

        current_descriptor->Reserved = 0;
    }

    // IDT and IDT descriptor are now filled and referenced in core context
    // core main loop will write IDTR during init
}
