#include "handle_dsm.h"

#include "cores.h"
#include "Protocol.h"
#include "asr/dsm.h"

#define MAX_DSM_EVENTS_PER_PACKET 64

static EFI_STATUS check_core_id(UINT64 core_id, ConnectionContext* ctx) {
    if (core_id >= MAX_CORE_COUNT) {
        FormatPrint(L"Core id %u is out of range, only max %u cores are supported.\n", core_id, MAX_CORE_COUNT);
        send_status(0x201, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    CoreContext* context = &core_contexts[core_id];
    if (context->present != 1) {
        FormatPrint(L"Core id %u is not present on this CPU.\n", core_id);
        send_status(0x202, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (context->started != 1) {
        FormatPrint(L"Core id %u is not started.\n", core_id);
        send_status(0x203, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_dsm_buffer(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_GETDSMBUFFER message.\n");
    EFI_STATUS status = EFI_SUCCESS;
    if (payload_length < 1 * sizeof(UINT64)) {
        FormatPrint(L"MSG_GETDSMBUFFER is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT64* payload_u64 = (UINT64*)payload;
    UINT64 core_id = payload_u64[0];

    status = check_core_id(core_id, ctx);
    if (status != EFI_SUCCESS) {
        return status;
    }

    CoreContext* context = &core_contexts[core_id];
    UINT64* response_u64 = (UINT64*)payload_buffer;
    UINTN min_response_size = 2 * sizeof(UINT64);

    if (context->dsm_control == NULL) {
        // DSM not initialized or available
        response_u64[0] = 0x0; // IBS not initialized
        response_u64[1] = 0x0; // file header size, no header sent
        
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_DSMBUFFER, payload_buffer, min_response_size, TRUE);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to construct message: %r.\n", status);
            return status;
        }

        status = send_message(response_buffer, min_response_size + HEADER_SIZE, ctx);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to send message: %r.\n", status);
            return status;
        }

        return EFI_SUCCESS;
    }

    response_u64[0] = 0x1; // flags
    const UINTN file_header_size = sizeof(context->dsm_control->file_header);
    response_u64[1] = file_header_size; // file header size
    CopyMem(response_u64 + 2, &context->dsm_control->file_header, file_header_size);
    min_response_size += file_header_size;

    UINT64 events_to_send = context->dsm_control->file_header.num_items;
    if (events_to_send == 0) {
        // no entries stored -> send no events
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_DSMBUFFER, payload_buffer, min_response_size, TRUE);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to construct message: %r.\n", status);
            return status;
        }

        status = send_message(response_buffer, min_response_size + HEADER_SIZE, ctx);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to send message: %r.\n", status);
            return status;
        }

        return EFI_SUCCESS;
    }

    UINTN start_index = 0;
    while (events_to_send > 0) {
        UINT64 iteration_events_to_send = MIN(MAX_DSM_EVENTS_PER_PACKET, events_to_send);
        UINT64 entries_copied = (UINT64)copy_dsm_entries(context, payload_buffer + min_response_size, start_index, iteration_events_to_send);
        DSM_file_header* payload_header = (DSM_file_header*)(&response_u64[2]);
        // if copy_dsm_entries works correctly this shouldn't underflow
        // still, defend against it
        if (entries_copied <= events_to_send) {
            events_to_send -= entries_copied;
        } else {
            events_to_send = 0;
        }
        start_index += entries_copied;
        payload_header->num_items = entries_copied;

        BOOLEAN last_packet = events_to_send == 0;
        const UINTN response_size = min_response_size + entries_copied * sizeof(DSMEvent);
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_DSMBUFFER, payload_buffer, response_size, last_packet);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to construct message: %r.\n", status);
            return status;
        }

        status = send_message(response_buffer, response_size + HEADER_SIZE, ctx);
        if (EFI_ERROR(status)) {
            FormatPrintDebug(L"Unable to send message: %r.\n", status);
            return status;
        }
    }

    return EFI_SUCCESS;
}
