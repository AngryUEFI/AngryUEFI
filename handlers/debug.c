#include "debug.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

#include "Protocol.h"
#include "AngryUEFI.h"

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
