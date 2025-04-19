#include "debug.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>


#include "Protocol.h"
#include "AngryUEFI.h"
#include "system/smp.h"

EFI_STATUS handle_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling PING message.\n");
    if (payload_length < 4) {
        FormatPrintDebug(L"PING is too short, need at least 4 Bytes, got %u.\n", payload_length);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 ping_size = *(UINT32*)payload;
    if (ping_size != payload_length - 4) {
        FormatPrintDebug(L"Mismatch in PING package, expected %u Bytes, got %u.\n", payload_length - 4, ping_size);
        return EFI_INVALID_PARAMETER;
    }

    const UINTN response_size = ping_size + 4 + HEADER_SIZE;
    *(UINT32*)payload_buffer = ping_size;
    CopyMem(payload_buffer + 4, payload + 4, ping_size);
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_multi_ping(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MULTIPING message.\n");
    if (payload_length < 8) {
        FormatPrintDebug(L"MULTIPING is too short, need at least 4 Bytes, got %u.\n", payload_length);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 ping_count = ((UINT32*)payload)[0];
    UINT32 ping_size = ((UINT32*)payload)[1];
    if (ping_size != payload_length - 8) {
        FormatPrintDebug(L"Mismatch in MULTIPING package, expected %u Bytes, got %u.\n", payload_length - 8, ping_size);
        return EFI_INVALID_PARAMETER;
    }

    const UINTN response_size = ping_size + 4 + HEADER_SIZE;
    *(UINT32*)payload_buffer = ping_size;
    CopyMem(payload_buffer + 4, payload + 8, ping_size);
    for (int i = 1; i < ping_count; i++) {
        EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, FALSE);
        if (EFI_ERROR(Status)) {
            FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
            return Status;
        }

        Status = send_message(response_buffer, response_size, ctx);
        if (EFI_ERROR(Status)) {
            FormatPrintDebug(L"Unable to send message: %r.\n", Status);
            return Status;
        }
    }

    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_PONG, payload_buffer, ping_size + 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_msg_size(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling GETMSGSIZE message.\n");

    const UINTN response_size = 4 + HEADER_SIZE;
    // first 4 bytes of payload are the payload size
    *(UINT32*)payload_buffer = payload_length - 4;
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_MSGSIZE, payload_buffer, 4, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, response_size, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_reboot(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_REBOOT message.\n");
    if (payload_length < 4) {
        FormatPrintDebug(L"MSG_REBOOT is too short, need at least 4 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 options = ((UINT32*)payload)[0];
    BOOLEAN warm_reset = ((options & 0x01) == 1);

    send_status(0x0, NULL, ctx);
    
    if (warm_reset)
        gRT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
    else
        gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

    return EFI_SUCCESS;
}
