#include "handle_ibs.h"

#include "cores.h"
#include "Protocol.h"
#include "asr/ibs.h"

#define MAX_IBS_EVENTS_PER_PACKET 64

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

EFI_STATUS handle_get_ibs_buffer(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_GETISBBUFFER message.\n");
    EFI_STATUS status = EFI_SUCCESS;
    if (payload_length < 3 * sizeof(UINT64)) {
        FormatPrint(L"MSG_GETISBBUFFER is too short, need at least 24 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT64* payload_u64 = (UINT64*)payload;
    UINT64 core_id = payload_u64[0];
    UINT64 start_index = payload_u64[1];
    UINT64 entry_count = payload_u64[2];

    status = check_core_id(core_id, ctx);
    if (status != EFI_SUCCESS) {
        return status;
    }

    CoreContext* context = &core_contexts[core_id];
    UINT64* response_u64 = (UINT64*)payload_buffer;
    const UINTN min_response_size = 4 * sizeof(UINT64);

    if (context->ibs_control == NULL) {
        // IBS not initialized or available
        response_u64[0] = 0x0; // IBS not initialized
        response_u64[1] = 0; // total stored events
        response_u64[2] = 0; // max stored events
        response_u64[3] = 0; // number of events in response
        
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_IBSBUFFER, payload_buffer, min_response_size, TRUE);
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

    const UINT64 stored_events = (UINT64)get_current_ibs_event_count(context);
    response_u64[0] = 0x1; // flags
    response_u64[1] = stored_events; // total stored events
    response_u64[2] = (UINT64)get_max_ibs_event_count(context); // max stored events
    response_u64[3] = 0; // number of events in response

    if (entry_count == 0) {
        // 0 -> request everything
        entry_count = stored_events;
    }
    UINT64 events_to_send = MIN(entry_count, stored_events - start_index);
    if (stored_events <= start_index || entry_count == 0 || events_to_send == 0) {
        // request out of bound -> send no events
        // no entries requested -> send no events
        // arguments lead to no events selected -> send no events
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_IBSBUFFER, payload_buffer, min_response_size, TRUE);
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

    while (events_to_send > 0) {
        UINT64 iteration_events_to_send = MIN(MAX_IBS_EVENTS_PER_PACKET, events_to_send);
        UINT64 entries_copied = (UINT64)copy_ibs_entries(context, payload_buffer + min_response_size, start_index, iteration_events_to_send);
        // if copy_ibs_entries works correctly this shouldn't underflow
        // still, defend against it
        if (entries_copied <= events_to_send) {
            events_to_send -= entries_copied;
        } else {
            events_to_send = 0;
        }
        start_index += entries_copied;
        response_u64[3] = entries_copied;

        BOOLEAN last_packet = events_to_send == 0;
        const UINTN response_size = min_response_size + entries_copied * sizeof(IBSEvent);
        status = construct_message(response_buffer, sizeof(response_buffer), MSG_IBSBUFFER, payload_buffer, response_size, last_packet);
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
