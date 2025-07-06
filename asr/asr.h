#ifndef ASR_H
#define ASR_H

#include <Base.h>

#include "handlers/cores.h"

// CoreContext is passed, just in case we at some point want core specifc ASRs
SMP_SAFE void* get_asr_for_index(CoreContext* context, UINT64 asr_index);

// this is implemented in ASM and has up to 7 register arguments
// RAX - index to call
// RDI - core context
// RSI - argument 1
// RDX - argument 2
// RCX - argument 3
// R8 - argument 4
// R9 - argument 5
// stack arguments are passed to the ASR as well
// return value in RAX
// RBX, RSP, RBP, R12, R13, R14 and R15 are preserved
// other registers are potentially changed
// this is essentially the System V ABI with RAX as additional index argument

// ASRs can define their arguments as normal in C
// this function should only be called from ASM, call the ASR via header from C
SMP_SAFE void* call_asr_for_index(void);

// allow for access from ASM
#pragma pack(push, 1)
typedef struct ASREntry_s {
    UINT64 index;
    void* ptr;
} ASREntry;
#pragma pack(pop)

// add your own ASR entry in asr.c

SMP_SAFE ASREntry* get_default_asr_registry();

// ASR rules
// 1. Must be SMP_SAFE
// 2. Must take CoreContext as first argument
// 3. Must not use arguments > 8 bytes (passed not in registers as usual)
// 4. Must not return value > 8 bytes (passed not in RAX as usual)


#endif /* ASR_H */
