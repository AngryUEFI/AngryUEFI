#include "Protocol.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

#include "AngryUEFI.h"
#include "handlers/debug.h"
#include "handlers/ucode.h"

UINT8 payload_buffer[RESPONSE_PAYLOAD_SIZE];
UINT8 response_buffer[RESPONSE_BUFFER_SIZE];

EFI_STATUS handle_message(UINT8* message, UINTN message_length, ConnectionContext* ctx) {
    EFI_STATUS Status = EFI_SUCCESS;
    if (message_length < 8) {
        FormatPrint(L"Message too small, got %u Bytes, need at least 8.\n", message_length);
        return EFI_INVALID_PARAMETER;
    }

    MetaData meta_data;
    CopyMem(&meta_data, message, sizeof(MetaData));
    if (meta_data.Major != 1 || meta_data.Minor != 0) {
        FormatPrint(L"Unsupported version %u.%u, only 1.0 is supported.\n", meta_data.Major, meta_data.Minor);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 message_type = ((UINT32*)message)[1];
    const UINTN payload_offset = sizeof(MetaData) + sizeof(message_type);

    switch (message_type) {
        case MSG_PING:
            Status = handle_ping(message + payload_offset, message_length - payload_offset, ctx);
            if (EFI_ERROR(Status)) {
                FormatPrint(L"Unable to handle PING message: %r\n", Status);
                return Status;
            }
            break;
        case MSG_MULTIPING:
            Status = handle_multi_ping(message + payload_offset, message_length - payload_offset, ctx);
            if (EFI_ERROR(Status)) {
                FormatPrint(L"Unable to handle MULTIPING message: %r\n", Status);
                return Status;
            }
            break;
        case MSG_GETMSGSIZE:
            Status = handle_get_msg_size(message + payload_offset, message_length - payload_offset, ctx);
            if (EFI_ERROR(Status)) {
                FormatPrint(L"Unable to handle GETMSGSIZE message: %r\n", Status);
                return Status;
            }
            break;
        default:
            FormatPrint(L"Unknown message type 0x%08X\n", message_type);
            return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

EFI_STATUS construct_message(UINT8* message_buffer, UINTN buffer_capacity, UINT32 message_type, UINT8* payload, UINTN payload_length, BOOLEAN last_message) {
    FormatPrint(L"Constructing message with %u paylength and type 0x%08X.\n", payload_length, message_type);
    if (buffer_capacity < payload_length + 12) {
        FormatPrint(L"Not enough space in response buffer, need %u, got %u\n", payload_length + 12, buffer_capacity);
        return EFI_INVALID_PARAMETER;
    }

    ((UINT32*)message_buffer)[0] = payload_length + 8;
    MetaData meta_data = {.Major = 1, .Minor = 0, .Control = (last_message)?0:1, .Reserved = 0};
    CopyMem(message_buffer + 4, &meta_data, sizeof(meta_data));
    ((UINT32*)message_buffer)[2] = message_type;
    CopyMem(message_buffer + 12, payload, payload_length);

    return EFI_SUCCESS;
}
