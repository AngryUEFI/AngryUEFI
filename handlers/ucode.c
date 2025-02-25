#include "ucode.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "Protocol.h"
#include "AngryUEFI.h"
#include "stubs.h"

UcodeContainer ucodes[UCODE_SLOTS] = {0};

EFI_STATUS handle_send_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling SENDUCODE message.\n");
    if (payload_length < 8) {
        FormatPrint(L"SENDUCODE is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 target_slot = ((UINT32*)payload)[0];
    UINT32 ucode_size = ((UINT32*)payload)[1];

    if (target_slot > UCODE_SLOTS - 1) {
        FormatPrint(L"Invalid target slot, got %u, max %u.\n", target_slot, UCODE_SLOTS - 1);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (ucodes[target_slot].ucode == NULL) {
        ucodes[target_slot].ucode = AllocateZeroPool(UCODE_SIZE);
        if (ucodes[target_slot].ucode == NULL) {
            FormatPrint(L"Unable to allocate memory for ucode.\n");
            send_status(0x3, FormatBuffer, ctx);
            return EFI_INVALID_PARAMETER;
        }
    }
    ucodes[target_slot].length = ucode_size;
    CopyMem(ucodes[target_slot].ucode, payload + 8, ucode_size);
    
    send_status(0x0, NULL, ctx);

    return EFI_SUCCESS;
}

void flip_bit(UINT8* ucode, UINT64 ucode_length, UINT32 position) {
    if (position / 8 >= ucode_length)
        return;
    ucode[position / 8] ^= 1 << (position % 8);
}

EFI_STATUS handle_flip_bits(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling FLIPBITS message.\n");
    if (payload_length < 8) {
        FormatPrint(L"FLIPBITS is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 target_slot = ((UINT32*)payload)[0];
    UINT32 num_flips = ((UINT32*)payload)[1];

    if (target_slot > UCODE_SLOTS - 1) {
        FormatPrint(L"Invalid target slot, got %u, max %u.\n", target_slot, UCODE_SLOTS - 1);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }
    if (ucodes[target_slot].ucode == NULL) {
        FormatPrint(L"Slot %u is empty\n", target_slot);
        send_status(0x3, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }
    
    if (target_slot != 1) {
        CopyMem(ucodes[1].ucode, ucodes[target_slot].ucode, ucodes[target_slot].length);
        ucodes[1].length = ucodes[target_slot].length;
    }

    UINT32* flips = ((UINT32*)payload) + 2;
    for (UINTN i = 0; i < num_flips; i++) {
        flip_bit(ucodes[1].ucode, ucodes[target_slot].length, flips[i]);
    }
    
    send_status(0x0, NULL, ctx);

    return EFI_SUCCESS;
}

EFI_STATUS handle_apply_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling MSG_APPLYUCODE message.\n");
    if (payload_length < 8) {
        FormatPrint(L"MSG_APPLYUCODE is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 target_slot = ((UINT32*)payload)[0];
    // UINT32 options = ((UINT32*)payload)[1];
    // TODO: handle options, ignored for now
    if (ucodes[target_slot].ucode == NULL || ucodes[target_slot].length == 0) {
        FormatPrint(L"Target slot %u is empty.\n", target_slot);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT8* ucode = ucodes[target_slot].ucode;
    UINT64 ret = apply_ucode_simple(ucode);
    *(UINT64*)payload_buffer = ret;

    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_UCODERESPONSE, payload_buffer, 8, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, 8 + HEADER_SIZE, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_read_msr(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    Print(L"Handling MSG_READMSR message.\n");
    if (payload_length < 4) {
        FormatPrint(L"MSG_READMSR is too short, need at least 4 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT32 target_msr = *(UINT32*)payload;

    UINT32 outputs[2] = {0};
    UINT64 ret = read_msr_stub(target_msr);
    outputs[0] = ret & 0xFFFFFFFF;
    outputs[1] = ret >> 32;
    FormatPrint(L"EAX: 0x%08X, EDX: 0x%08X.\n", outputs[0], outputs[1]); 

    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_MSRRESPONSE, (UINT8*)outputs, sizeof(outputs), TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, sizeof(outputs) + HEADER_SIZE, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
