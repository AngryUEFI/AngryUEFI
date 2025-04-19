#ifndef FAULT_HANDLING_H
#define FAULT_HANDLING_H

#include <Base.h>

#include "handlers/cores.h"

// access from assembly, do not change order
// only accessed from assembly in fault_handling_stubs.s
// AngryCAT python code might read some offsets, check that code after edits here
// prefer to add new fields at the end, AngryCAT can deal with longer buffers
// zeroed unless fault occurs
#pragma pack(push,1)
typedef struct CoreFaultInfo_s {
    // zeroed when starting a new job
    UINT64 fault_occured; // + 0x0

    UINT64 fault_number; // + 0x8
    UINT64 error_code; // + 0x10
    UINT64 old_rip; // + 0x18

    UINT64 rax_value; // + 0x20
    UINT64 rbx_value; // + 0x28
    UINT64 rcx_value; // + 0x30
    UINT64 rdx_value; // + 0x38
    UINT64 rsi_value; // + 0x40
    UINT64 rdi_value; // + 0x48
    // upon entry to ISR
    UINT64 rsp_value; // + 0x50
    UINT64 rbp_value; // + 0x58
    UINT64 r8_value; // + 0x60
    UINT64 r9_value; // + 0x68
    UINT64 r10_value; // + 0x70
    UINT64 r11_value; // + 0x78
    UINT64 r12_value; // + 0x80
    UINT64 r13_value; // + 0x88
    UINT64 r14_value; // + 0x90
    UINT64 r15_value; // + 0x98

    UINT64 rflags_value; // + 0xA0

    UINT64 cr0_value; // + 0xA8
    UINT64 cr2_value; // + 0xB0
    UINT64 cr3_value; // + 0xB8
    UINT64 cr4_value; // + 0xC0

    UINT64 cs_value; // + 0xC8
    // before switch to ISR
    UINT64 original_rsp; // + 0xD0
} CoreFaultInfo;
#pragma pack(pop)

void init_fault_handlers_on_core(CoreContext* context);
SMP_SAFE void write_idt_on_core(CoreContext* context);

// IDT descriptor for x64 (as defined by the CPU)
#pragma pack(push,1)
typedef struct IDT_DESCRIPTOR_s {
    UINT16 Limit;
    UINT64 Base;
} IDT_DESCRIPTOR;
#pragma pack(pop)

// IDT entry for x64
#pragma pack(push,1)
typedef struct {
    UINT16 OffsetLow;      // Bits 0-15 of handler address.
    UINT16 SegmentSelector;// Code segment selector.
    UINT8  Ist;            // Interrupt Stack Table offset.
    UINT8  Attributes;     // Type and attributes.
    UINT16 OffsetMid;      // Bits 16-31 of handler address.
    UINT32 OffsetHigh;     // Bits 32-63 of handler address.
    UINT32 Reserved;
} IDT_ENTRY;
#pragma pack(pop)

// 0x38 seems to be default for this build/environment
#define CODE_SEGMENT 0x38

#endif /* FAULT_HANDLING_H */
