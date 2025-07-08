#ifndef HANDLE_IBS_H
#define HANDLE_IBS_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_get_ibs_buffer(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#endif /* HANDLE_IBS_H */
