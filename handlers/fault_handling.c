#include "fault_handling.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "handlers/cores.h"
#include "handlers/fault_handling_stubs.h"

void init_fault_handlers_on_core(CoreContext* context) {
    CoreFaultInfo* fault_info = AllocateZeroPool(sizeof(CoreFaultInfo));

    context->fault_info = fault_info;

    const UINT64 fault_handlers_length = (UINT64)&proto_handlers_end - (UINT64)&proto_fault_stub_start;
    UINT8* fault_handlers = AllocatePool(fault_handlers_length);
    
    CopyMem(fault_handlers, &proto_fault_stub_start, fault_handlers_length);
    // first 8 Bytes are the core context pointer
    *(UINT64*)fault_handlers = (UINT64)context;
    
    // there is a minimum amount of IDT entries between 0x20 and 0xff
    const UINTN highest_fault_handler = 0xff;

    // allocate space for an IDT for this core
    IDT_ENTRY* idt = AllocateZeroPool(sizeof(IDT_ENTRY) * (highest_fault_handler + 1));

    // set IDT descriptor members
    context->idt_base = idt;
    context->idt_limit = highest_fault_handler;

    // fill the IDT with our handlers
    for (UINTN i = 0; i <= highest_fault_handler; i++) {
        // fill all entries in IDT with our fallback_handler
        UINT64 handler_address = (UINT64)fallback_handler;
        if (i < proto_fault_handlers_count) {
            // we have an actual handler for this fault
            // offset from start of handler buffer
            UINT64 handler_offset = proto_fault_handlers_offsets[i];
            // absolute address for this core
            handler_address = (UINT64)fault_handlers + handler_offset;
        }

        IDT_ENTRY* current_descriptor = &idt[i];

        current_descriptor->OffsetLow = (UINT16)(handler_address & 0xFFFF);
        current_descriptor->OffsetMid = (UINT16)((handler_address >> 16) & 0xFFFF);
        current_descriptor->OffsetHigh = (UINT32)((handler_address >> 32) & 0xFFFFFFFF);

        current_descriptor->SegmentSelector = CODE_SEGMENT;
        current_descriptor->Ist = 0; // use current stack
        current_descriptor->Attributes = 0x8E;

        current_descriptor->Reserved = 0;
    }

    // IDT is now filled and referenced in core context
    // core main loop will write IDTR during init
}

SMP_SAFE void write_idt_on_core(CoreContext* context) {
    set_idtr(context->idt_base, (UINT16)context->idt_limit);
}
