#ifndef STUBS_H
#define STUBS_H

#include <Base.h>

UINT64 test_stub(UINT64 a, UINT64 b);
UINT64 read_msr_stub(UINT32 target_msr);
UINT64 apply_ucode_simple(UINT8* ucode, UINT64* interrupt_value);
UINT64 apply_ucode_restore(UINT8* ucode, UINT64* interrupt_value);

UINT8* read_idt_position();
void write_idt_position();

void gpf_handler();

#endif /* STUBS_H */
