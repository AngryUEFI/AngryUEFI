#include "Protocol.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

#include "AngryUEFI.h"

#define RESPONSE_PAYLOAD_SIZE 1024
UINT8 payload_buffer[RESPONSE_PAYLOAD_SIZE];
#define RESPONSE_BUFFER_SIZE RESPONSE_PAYLOAD_SIZE + 12 // 1024 Bytes Payload + 4 Bytes Length + 4 Bytes Metadata + 4 Bytes Message Type
UINT8 response_buffer[RESPONSE_BUFFER_SIZE];

#define HEADER_SIZE 12

#pragma pack(1)
typedef struct MetaData_s {
    UINT8 Major;
    UINT8 Minor;
    UINT8 Control;
    UINT8 Reserved;
} MetaData;
#pragma pack()

enum MessageType {
    MSG_PING = 0x1,
    MSG_MULTIPING = 0x2,
    MSG_GETMSGSIZE = 0x3,

    MSG_STATUS = 0x80000000,
    MSG_PONG = 0x80000001,
    MSG_MSGSIZE = 0x80000003,
};

EFI_STATUS handle_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_multi_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_msg_size(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

EFI_STATUS construct_message(UINT8* message_buffer, UINTN buffer_capacity, UINT32 message_type, UINT8* payload, UINTN payload_length, BOOLEAN last_message);

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

EFI_STATUS handle_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling PING message.\n");
    if (payload_length < 4) {
        FormatPrint(L"PING is too short, need at least 4 Bytes, got %u.\n", payload_length);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 ping_size = *(UINT32*)payload;
    if (ping_size != payload_length - 4) {
        FormatPrint(L"Mismatch in PING package, expected %u Bytes, got %u.\n", payload_length - 4, ping_size);
        return EFI_INVALID_PARAMETER;
    }

    const UINTN response_size = ping_size + 4 + HEADER_SIZE;
    *(UINT32*)payload_buffer = ping_size;
    CopyMem(payload_buffer + 4, payload + 4, ping_size);
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_multi_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling MULTIPING message.\n");
    if (payload_length < 8) {
        FormatPrint(L"MULTIPING is too short, need at least 4 Bytes, got %u.\n", payload_length);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 ping_count = ((UINT32*)payload)[0];
    UINT32 ping_size = ((UINT32*)payload)[1];
    if (ping_size != payload_length - 8) {
        FormatPrint(L"Mismatch in MULTIPING package, expected %u Bytes, got %u.\n", payload_length - 8, ping_size);
        return EFI_INVALID_PARAMETER;
    }

    const UINTN response_size = ping_size + 4 + HEADER_SIZE;
    *(UINT32*)payload_buffer = ping_size;
    CopyMem(payload_buffer + 4, payload + 8, ping_size);
    for (int i = 1; i < ping_count; i++) {
        EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, FALSE);
        if (EFI_ERROR(Status)) {
            FormatPrint(L"Unable to construct message: %r.\n", Status);
            return Status;
        }

        Status = send_message(response_buffer, response_size, ctx);
        if (EFI_ERROR(Status)) {
            FormatPrint(L"Unable to send message: %r.\n", Status);
            return Status;
        }
    }

    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_msg_size(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling GETMSGSIZE message.\n");

    const UINTN response_size = 4 + HEADER_SIZE;
    // first 4 bytes of payload are the payload size
    *(UINT32*)payload_buffer = payload_length - 4;
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_MSGSIZE, payload_buffer, 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
