#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_message(void* message, UINTN message_length, ConnectionContext* ctx);

#endif /* PROTOCOL_H */
