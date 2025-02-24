#include "Protocol.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

#include "AngryUEFI.h"

EFI_STATUS handle_message(void* message, UINTN message_length, ConnectionContext* ctx) {
    EFI_STATUS Status = EFI_SUCCESS;

    Status = send_message(message, message_length, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
