#include "ucode.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

#include "Protocol.h"
#include "AngryUEFI.h"

EFI_STATUS handle_send_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    return EFI_SUCCESS;
}

EFI_STATUS handle_flip_bits(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    return EFI_SUCCESS;
}

EFI_STATUS handle_apply_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    return EFI_SUCCESS;
}

EFI_STATUS handle_read_msr(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    return EFI_SUCCESS;
}
