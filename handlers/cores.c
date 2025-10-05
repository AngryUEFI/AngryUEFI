#include "cores.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "system/smp.h"
#include "stubs.h"
#include "Protocol.h"
#include "asr/asr.h"
#include "asr/ibs.h"
#include "asr/dsm.h"

#include "system/fault_handling.h"
#include "system/fault_handling_stubs.h"
#include "system/paging_stubs.h"
#include "system/paging.h"

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

    // zero out previous fault
    if (context->fault_info->fault_occured != 0) {
        ZeroMem(context->fault_info, sizeof(CoreFaultInfo));
    }

    call_core_functions(context);

    // job function filled the context
    // indicate we are ready for the next job
    context->ready = 1;

end:
    unlock_context(context);
    return;
}

// fault recovery mechanism:
// 1. core start calls core_main_loop_entry_point from UEFI
// 2. core_main_loop_entry_point calls core_main_loop_stub_wrapper
// 3. core_main_loop_stub_wrapper sets context->recovery_rsp to current RSP
// 4. core_main_loop_stub_wrapper jumps to core_main_loop
//
// 5. fault during test execution executes core specific fault handler
// 6. fault handler returns (iretq) to core_main_loop_stub_recovery
// 7. core_main_loop_stub_recovery restores context->recovery_rsp into RSP
// 8. core_main_loop_stub_recovery jumps to core_main_loop

// core_main_loop is re-entrant, inits core again after recovery
// only next job clears fault status, allows core 0 to retrieve fault info
// RSP needs to be reset to original value, else stack would grow for each recovered fault

// EFIAPI, beause it is started from EFI APIs
// calls into stub
static EFIAPI void core_main_loop_entry_point(void* arg) {
    CoreContext* context = (CoreContext*)arg;
    set_cr3(context->cr3_value);
    core_main_loop_stub_wrapper(context);
}

// this is the main loop of a core
// all APs spin in this
// core 0 does not have this main loop, it runs the network stack
// after recovery fault handler calls into this
// must be re-entrant as fault handlers will call into this
void core_main_loop(CoreContext* context) {
    // we enter this function with a locked context

    // set core specific IDTR
    write_idt_on_core(context);

    // update cr3
    context->cr3_value = get_cr3();

    // (re-)init IBS
    init_ibs_on_core(context);

    // (re-)init DSM
    init_dsm_on_core(context);

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

SMP_SAFE void call_core_functions(CoreContext* context) {
    for (UINTN i = 0; i < CORE_FUNCTION_SLOTS; i++) {
        CoreFunction func = context->core_functions[i].func;
        if (func != NULL) {
            func(context, context->core_functions[i].arg);
        }
    }
}

BOOLEAN wait_on_ap_exec(CoreContext* context, UINT64 timeout) {
    UINTN iterations = 0;
    while (1) {
        if (context->lock == 0) {
            // potentially unlocked, try to get a lock
            if (try_lock_context(context)) {
                if (context->ready == 1 && context->job_queued == 0) {
                    // job finished
                    unlock_context(context);
                    return FALSE;
                }
                unlock_context(context);
            }
            // AP just grabbed the lock, check again next iteration
        }

        // timeout handling
        if (timeout != 0) {
            gBS->Stall(1000); // stall 1ms
            if (++iterations >= timeout) {
                return TRUE;
            }
        } else {
            CpuPause();
        }
    }
}

EFI_STATUS acquire_core_lock_for_job(UINT64 core_id, ConnectionContext* ctx) {
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

    if (!try_lock_context(context)) {
        FormatPrint(L"Core id %u context is already locked, invalid state.\n", core_id);
        send_status(0x204, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (context->ready != 1) {
        unlock_context(context);
        FormatPrint(L"Core id %u is not ready to accept a job.\n", core_id);
        send_status(0x205, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (context->job_queued == 1) {
        unlock_context(context);
        FormatPrint(L"Core id %u already has a job queued.\n", core_id);
        send_status(0x206, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

SMP_SAFE void clear_core_functions(CoreContext* context) {
    ZeroMem(context->core_functions, sizeof(context->core_functions));
}

static EFI_STATUS start_core(UINT64 core_id, EFI_AP_PROCEDURE core_main, ConnectionContext* ctx) {
    EFI_STATUS status = EFI_SUCCESS;
    
    CoreContext* context = &core_contexts[core_id];
    if (context->present != 1) {
        FormatPrint(L"Core id %u is not present on this CPU.\n", core_id);
        send_status(0x3, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (context->started == 1) {
        FormatPrint(L"Core id %u is already started.\n", core_id);
        send_status(0x4, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (!try_lock_context(context)) {
        FormatPrint(L"Core id %u already has a locked context.\n", core_id);
        send_status(0x5, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    init_fault_handlers_on_core(context);
    // uses core 0 cr3 as base
    void* new_cr3 = create_minimal_paging(get_cr3());
    context->cr3_value = new_cr3;
    // on core start function will load this value

    status = start_on_core(core_id, core_main, context, &context->core_event, 0);
    if (status != EFI_SUCCESS) {
        unlock_context(context);
        FormatPrint(L"Core id %u can not be started: %r.\n", core_id, status);
        send_status(0x6, FormatBuffer, ctx);
        return status;
    }
    
    return EFI_SUCCESS;
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
            // no need to start again
            if (core_contexts[i].started == 1) {
                continue;
            }
            
            status = start_core(i, core_main_loop_entry_point, ctx);
            if (status != EFI_SUCCESS) {
                return status;
            }
        }
    } else {
        status = start_core(core_to_start, core_main_loop_entry_point, ctx);
        if (status != EFI_SUCCESS) {
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

    BOOLEAN dump_fault_info = FALSE;
    UINT64 flags = 0ull;
    flags |= (context->present & 0x1) << 0;
    flags |= (context->started & 0x1) << 1;
    flags |= (context->ready & 0x1) << 2;
    flags |= (context->job_queued & 0x1) << 3;
    flags |= (context->lock & 0x1) << 4;
    // fault info only gets inited once the core starts
    if (context->fault_info != NULL) {
        flags |= (context->fault_info->fault_occured & 0x1) << 5;
        if (context->fault_info->fault_occured != 0) {
            dump_fault_info = TRUE;
        }
    }
    
    payload_u64[0] = flags;
    payload_u64[1] = context->heartbeat;
    payload_u64[2] = get_tsc();
    payload_u64[3] = 0;

    UINTN response_size = 4 * sizeof(UINT64);

    if (dump_fault_info) {
        payload_u64[3] = sizeof(CoreFaultInfo);
        response_size += sizeof(CoreFaultInfo);
        CopyMem(&payload_u64[4], context->fault_info, sizeof(CoreFaultInfo));
    }

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
        current->core_id = i;
        
        if (i < core_count) {
            current->present = 1;
        }
        
        if (current->present != 0) {
            current->result_buffer = AllocateZeroPool(RESULT_BUFFER_SIZE);
            current->result_buffer_len = RESULT_BUFFER_SIZE;
            current->scratch_space = AllocateZeroPool(MACHINE_CODE_SCRATCH_SPACE_SIZE);
            current->scratch_space_len = MACHINE_CODE_SCRATCH_SPACE_SIZE;

            current->asr_gate = call_asr_for_index;
            current->asr_registry = get_default_asr_registry();

            init_ibs_for_context(current);
            init_dsm_for_context(current);
        }
    }

    // core 0 is always ready
    CoreContext* core_0 = &core_contexts[0];
    core_0->present = 1;
    core_0->started = 1;
    core_0->ready = 1;
    core_0->cr3_value = get_cr3();
}

void init_cores() {
    init_core_contexts();
}