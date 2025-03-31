#ifndef STUBS_H
#define STUBS_H

#include <Base.h>

#include "handlers/cores.h"

UINT64 test_stub(UINT64 a, UINT64 b);
UINT64 read_msr_stub(UINT32 target_msr);
UINT64 apply_ucode_simple(UINT8* ucode, UINT64* interrupt_value);
UINT64 apply_ucode_restore(UINT8* ucode, UINT64* interrupt_value);
void apply_ucode_execute_machine_code_simple(CoreContext* meta_data);
void apply_ucode_execute_machine_code_restore(CoreContext* meta_data);
// return rdtsc
UINT64 get_tsc();
// need to implement this ourselves, else need to modify build config
// currently only a dummy implementation which ASSERT()s is provided
UINT64 interlocked_compare_exchange_64(volatile UINT64* mem, UINT64 compare_value, UINT64 exchange_value);

UINT8* read_idt_position();
void write_idt_position();

void gpf_handler();

#endif /* STUBS_H */
