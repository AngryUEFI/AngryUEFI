#include "cores.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "smp.h"
#include "stubs.h"
#include "Protocol.h"

void lock_context(CoreContext* context) {
    // spin loop until we get the lock
    while (interlocked_compare_exchange_64(&context->lock, 0, 1) != 0) {
        // pause for a little bit
        CpuPause();
    }
}

BOOLEAN try_lock_context(CoreContext* context) {
    return interlocked_compare_exchange_64(&context->lock, 0, 1) == 0;
}

void unlock_context(CoreContext* context) {
    context->lock = 0;
    interlocked_compare_exchange_64(&context->lock, 1, 0);
}

static void handle_job(CoreContext* context) {
    // we have a job to handle
    // this means updating the context
    // also our job function might write to the context
    // try to lock the context so core 0 does not write to it anymore
    // if we can't get the lock, core 0 grabbed it again
    if (!try_lock_context(context)) {
        return;
    }

    // check that after locking there still is a job for us
    if (!context->job_queued) {
        goto end;
    }

    // accept the job
    context->ready = 0;
    context->job_queued = 0;

    PrepareFunction prepare = context->prepare_function;
    prepare(context->prepare_function_argument);

    JobFunction to_call = context->job_function;
    to_call(context->job_function_argument);

    // job function filled the context
    // indicate we are ready for the next job
    context->ready = 1;

end:
    unlock_context(context);
    return;
}

// this is the main loop of a core
// all APs spin in this
// core 0 does not have this main loop, it runs the network stack
// EFIAPI, beause it is started from EFI APIs
static EFIAPI void core_main_loop(void* arg) {
    CoreContext* context = (CoreContext*)arg;
    // we enter this function with a locked context

    // set the IDTR to the one from core 0
    // potential issue:
    // this assumes ucode was already inited, which inits
    // the custom GPF
    // if this did not occur, this will fail
    write_idt_position();

    // ready for work :)
    context->started = 1;
    context->ready = 1;
    context->job_queued = 0;

    unlock_context(context);

    for/*ever*/(;;) {
        // we are in a spin loop
        CpuPause();

        // we are alive!
        context->heartbeat = get_tsc();
        
        // core 0 is writing to our context
        // wait for it to finish
        if (context->lock) {
            continue;
        }

        // core 0 has written a job for us
        if (context->job_queued) {
            handle_job(context);
            // job is done, wait for next
            continue;
        }
    }
}

EFI_STATUS handle_start_core(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_STARTCORE message.\n");
    EFI_STATUS status = EFI_SUCCESS;
    if (payload_length < 8) {
        FormatPrint(L"MSG_STARTCORE is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT64 core_to_start = *(UINT64*)payload;

    if (core_to_start > MAX_CORE_COUNT) {
        FormatPrint(L"Core id %u is out of range, only %u max cores are supported.\n", core_to_start, MAX_CORE_COUNT);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (core_to_start == 0) {
        for (UINTN i = 1; i < get_available_cores(); i++) {
            // how did we even get here?
            if (core_contexts[i].present != 1) {
                FormatPrint(L"Starting all cores failed: core id %u is not present on this CPU.\n", i);
                send_status(0x3, FormatBuffer, ctx);
                return EFI_INVALID_PARAMETER;
            }
    
            // no need to start again
            if (core_contexts[i].started == 1) {
                continue;
            }
            
            CoreContext* context = &core_contexts[i];
            if (!try_lock_context(context)) {
                FormatPrint(L"Starting all cores failed: core id %u already has a locked context.\n", i);
                send_status(0x5, FormatBuffer, ctx);
                return EFI_INVALID_PARAMETER;
            }
            status = start_on_core(i, core_main_loop, context, &context->core_event, 0);
            if (status != EFI_SUCCESS) {
                unlock_context(context);
                FormatPrint(L"Starting all cores failed: core id %u can not be started: %r.\n", i, status);
                send_status(0x6, FormatBuffer, ctx);
                return status;
            }
        }
    } else {
        if (core_contexts[core_to_start].present != 1) {
            FormatPrint(L"Core id %u is not present on this CPU.\n", core_to_start);
            send_status(0x3, FormatBuffer, ctx);
            return EFI_INVALID_PARAMETER;
        }

        if (core_contexts[core_to_start].started == 1) {
            FormatPrint(L"Core id %u is already started.\n", core_to_start);
            send_status(0x4, FormatBuffer, ctx);
            return EFI_INVALID_PARAMETER;
        }

        CoreContext* context = &core_contexts[core_to_start];
        if (!try_lock_context(context)) {
            FormatPrint(L"Core id %u already has a locked context.\n", core_to_start);
            send_status(0x5, FormatBuffer, ctx);
            return EFI_INVALID_PARAMETER;
        }
        status = start_on_core(core_to_start, core_main_loop, context, &context->core_event, 0);
        if (status != EFI_SUCCESS) {
            unlock_context(context);
            FormatPrint(L"Core id %u can not be started: %r.\n", core_to_start, status);
            send_status(0x6, FormatBuffer, ctx);
            return status;
        }
    }

    send_status(0x0, NULL, ctx);

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_core_count(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_GETCORECOUNT message.\n");

    UINTN core_count = get_available_cores();

    *(UINT64*)payload_buffer = core_count;
    EFI_STATUS Status = construct_message(response_buffer, sizeof(response_buffer), MSG_CORECOUNTRESPONSE, payload_buffer, 8, TRUE);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to construct message: %r.\n", Status);
        return Status;
    }

    Status = send_message(response_buffer, 8 + HEADER_SIZE, ctx);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Unable to send message: %r.\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_core_status(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_GETCORESTATUS message.\n");
    if (payload_length < 8) {
        FormatPrint(L"MSG_GETCORESTATUS is too short, need at least 8 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT64 core_id = *(UINT64*)payload;

    if (core_id > MAX_CORE_COUNT) {
        FormatPrint(L"Core id %u is out of range, only %u max cores are supported.\n", core_id, MAX_CORE_COUNT);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    CoreContext* context = &core_contexts[core_id];
    return send_core_status(context, TRUE, ctx);
}

EFI_STATUS send_core_status(CoreContext* context, BOOLEAN is_last_message, ConnectionContext* ctx) {
    EFI_STATUS status = EFI_SUCCESS;
    UINT64* payload_u64 = (UINT64*)payload_buffer;

    UINT64 flags = 0ull;
    flags |= (context->present & 0x1) << 0;
    flags |= (context->started & 0x1) << 1;
    flags |= (context->ready & 0x1) << 2;
    flags |= (context->job_queued & 0x1) << 3;
    flags |= (context->lock & 0x1) << 4;
    
    payload_u64[0] = flags;
    payload_u64[1] = context->heartbeat;
    payload_u64[2] = get_tsc();

    const UINTN response_size = 3 * sizeof(UINT64);

    status = construct_message(response_buffer, sizeof(response_buffer), MSG_CORESTATUSRESPONSE, payload_buffer, response_size, is_last_message);
    if (EFI_ERROR(status)) {
        FormatPrint(L"Unable to construct message: %r.\n", status);
        return status;
    }

    status = send_message(response_buffer, response_size + HEADER_SIZE, ctx);
    if (EFI_ERROR(status)) {
        FormatPrint(L"Unable to send message: %r.\n", status);
        return status;
    }

    return EFI_SUCCESS;
}

CoreContext core_contexts[MAX_CORE_COUNT] = {0};

static void init_core_contexts() {
    UINTN core_count = get_available_cores();
    for (UINTN i = 0; i < MAX_CORE_COUNT; i++) {
        CoreContext* current = &core_contexts[i];
        current->result_buffer = AllocateZeroPool(RESULT_BUFFER_SIZE);
        current->result_buffer_len = RESULT_BUFFER_SIZE;
        current->scratch_space = AllocateZeroPool(MACHINE_CODE_SCRATCH_SPACE_SIZE);
        current->scratch_space_len = MACHINE_CODE_SCRATCH_SPACE_SIZE;
        current->core_id = i;
        if (i < core_count) {
            current->present = 1;
        }
    }

    // core 0 is always ready
    CoreContext* core_0 = &core_contexts[0];
    core_0->present = 1;
    core_0->started = 1;
    core_0->ready = 1;
}

void init_cores() {
    init_core_contexts();
}