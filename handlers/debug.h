#ifndef DEBUG_H
#define DEBUG_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_multi_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_msg_size(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_reboot(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_core_count(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#endif /* DEBUG_H */
