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
#include "handlers/ucode_execute.h"
#include "handlers/cores.h"

UINT8 payload_buffer[RESPONSE_PAYLOAD_SIZE];
UINT8 response_buffer[RESPONSE_BUFFER_SIZE];

EFI_STATUS handle_message(UINT8* message, UINTN message_length, ConnectionContext* ctx) {
    if (message_length < 8) {
        FormatPrintDebug(L"Message too small, got %u Bytes, need at least 8.\n", message_length);
        return EFI_INVALID_PARAMETER;
    }

    MetaData meta_data;
    CopyMem(&meta_data, message, sizeof(MetaData));
    if (meta_data.Major != 1 || meta_data.Minor != 0) {
        FormatPrintDebug(L"Unsupported version %u.%u, only 1.0 is supported.\n", meta_data.Major, meta_data.Minor);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 message_type = ((UINT32*)message)[1];
    const UINTN payload_offset = sizeof(MetaData) + sizeof(message_type);

    switch (message_type) {
        case MSG_PING:
            handle_ping(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_MULTIPING:
            handle_multi_ping(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_GETMSGSIZE:
            handle_get_msg_size(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_REBOOT:
            handle_reboot(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_SENDUCODE:
            handle_send_ucode(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_FLIPBITS:
            handle_flip_bits(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_APPLYUCODE:
            handle_apply_ucode(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_APPLYUCODEEXCUTETEST:
            handle_apply_ucode_execute_test(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_GETLASTTESTRESULT:
            handle_get_last_test_result(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_READMSR:
            handle_read_msr(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_READMSRONCORE:
            handle_read_msr_on_core(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_GETCORECOUNT:
            handle_get_core_count(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_STARTCORE:
            handle_start_core(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_GETCORESTATUS:
            handle_get_core_status(message + payload_offset, message_length - payload_offset, ctx);
            break;
        case MSG_SENDMACHINECODE:
            handle_send_machine_code(message + payload_offset, message_length - payload_offset, ctx);
            break;
        
        default:
            FormatPrintDebug(L"Unknown message type 0x%08X\n", message_type);
            send_message(FormatBuffer, 0x80000001, ctx);
            break;
    }

    return EFI_SUCCESS;
}

EFI_STATUS construct_message(UINT8* message_buffer, UINTN buffer_capacity, UINT32 message_type, UINT8* payload, UINTN payload_length, BOOLEAN last_message) {
    FormatPrintDebug(L"Constructing message with %u paylength and type 0x%08X.\n", payload_length, message_type);
    if (buffer_capacity < payload_length + 12) {
        FormatPrintDebug(L"Not enough space in response buffer, need %u, got %u\n", payload_length + 12, buffer_capacity);
        return EFI_INVALID_PARAMETER;
    }

    ((UINT32*)message_buffer)[0] = payload_length + 8;
    MetaData meta_data = {.Major = 1, .Minor = 0, .Control = (last_message)?0:1, .Reserved = 0};
    CopyMem(message_buffer + 4, &meta_data, sizeof(meta_data));
    ((UINT32*)message_buffer)[2] = message_type;
    CopyMem(message_buffer + 12, payload, payload_length);

    return EFI_SUCCESS;
}

EFI_STATUS send_status(UINT32 status_code, CHAR16* message, ConnectionContext* ctx) {
    UINTN response_size = 8;
    if (message != NULL) {
        response_size += StrSize(message);
        CopyMem(payload_buffer + 8, message, StrSize(message));
        ((UINT32*)payload_buffer)[1] = StrSize(message);
    } else {
        ((UINT32*)payload_buffer)[1] = 0;
    }

    *(UINT32*)payload_buffer = status_code;
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_STATUS, payload_buffer, response_size, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size + HEADER_SIZE, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
