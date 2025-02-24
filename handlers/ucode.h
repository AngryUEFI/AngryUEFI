#ifndef UCODE_H
#define UCODE_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_send_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_flip_bits(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_apply_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_read_msr(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#endif /* UCODE_H */
