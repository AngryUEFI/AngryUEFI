#ifndef HANDLE_DSM_H
#define HANDLE_DSM_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_get_dsm_buffer(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#endif /* HANDLE_DSM_H */
