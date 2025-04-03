#ifndef UCODE_H
#define UCODE_H

#include <Base.h>

#include "AngryUEFI.h"

void init_ucode();

EFI_STATUS handle_send_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_flip_bits(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_apply_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_read_msr(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_read_msr_on_core(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#pragma pack(push, 1)
typedef struct UcodeContainer_s {
    UINT8* ucode;
    UINT64 length;
} UcodeContainer;
#pragma pack(pop)

// 0 - known good
// 1 - target of mutation operations
// 2-9 - free use
#define UCODE_SLOTS 10
extern UcodeContainer ucodes[UCODE_SLOTS];

// referenced in stub.s
extern UINT8* original_ucode;

// big enough for family 0x19/Zen 5
#define UCODE_SIZE 5568

#endif /* UCODE_H */
