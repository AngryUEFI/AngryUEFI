#ifndef FAULT_HANDLING_H
#define FAULT_HANDLING_H

#include <Base.h>

#include "handlers/cores.h"

// access from assembly, do not change order
// only accessed from assembly in fault_handling_stubs.s
typedef struct CoreFaultInfo_s {
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
    UINT64 original_rsp; // + 0xD0
} CoreFaultInfo;

void init_fault_handlers_on_core(CoreContext* context);
SMP_SAFE void write_idt_on_core(CoreContext* context);

// IDT descriptor for x64 (as defined by the CPU)
#pragma pack(push,1)
typedef struct IDT_DESCRIPTOR_s {
    UINT16 Limit;
    UINT64 Base;
} IDT_DESCRIPTOR;
#pragma pack(pop)

#endif /* FAULT_HANDLING_H */
