#ifndef CORES_H
#define CORES_H

#include <Base.h>
#include <Library/BaseMemoryLib.h>

#include "AngryUEFI.h"

void init_cores();

#define RESULT_BUFFER_SIZE 1*1024
#define MACHINE_CODE_SCRATCH_SPACE_SIZE 4*1024
#define CORE_FUNCTION_SLOTS 3

typedef struct CoreContext_s CoreContext;

// no-op annotation to signal a function is safe to call from an AP
// no properties are actually enforced, don't shoot your foot off
// the function guarantees:
// 1. does not use global/static state
// 2. does not call functions not SMP_SAFE
// 3. can run in parallel on APs
// 4. will never acquire locks
// the function expects:
// 1. all passed arguments, pointers and otherwise, are safe to modify on APs
// 2. passed pointers, e.g. CoreContext, are locked if needed
// 3. passed function pointers are SMP_SAFE
#define SMP_SAFE

EFI_STATUS handle_start_core(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_core_status(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_core_count(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

EFI_STATUS send_core_status(CoreContext* context, BOOLEAN is_last_message, ConnectionContext* ctx);

SMP_SAFE void call_core_functions(CoreContext* context);
BOOLEAN wait_on_ap_exec(CoreContext* context, UINT64 timeout);
EFI_STATUS acquire_core_lock_for_job(UINT64 core_id, ConnectionContext* ctx);

SMP_SAFE void clear_core_functions(CoreContext* context);

// actual core main loop
// does not return
// not really SMP_SAFE, should only be called during
// core startup or fault recovery
// core_main_loop_stub_wrapper calls into this
// during startup
// called after fault recovery
void core_main_loop(CoreContext* context);

// lock the context
// will spin until lock is available
// yes, this can deadlock your code
void lock_context(CoreContext* context);
void unlock_context(CoreContext* context);
// try to get a lock, fail if context is already locked
BOOLEAN try_lock_context(CoreContext* context);

typedef SMP_SAFE void (*CoreFunction)(CoreContext* argument, void* arg);

typedef struct CoreFunctionCall_s {
    CoreFunction func;
    void* arg;
} CoreFunctionCall;

typedef struct CoreFaultInfo_s CoreFaultInfo;
typedef struct ASREntry_s ASREntry;
typedef struct IBSControl_s IBSControl;
typedef struct DSMControl_s DSMControl;

// each core gets its own copy of this control structure
// jobs operate on this structure
// each core can only run a single job at a time
// so this is also the job control structure
// writing into this structure from a different core can lead to UB
// usually only core 0 will write job information when a job is not running
// while a job is running, only that job will write this structure
// core 0 might read this structure to get the current state when requested
#pragma pack(push,1)
typedef struct CoreContext_s {
    // do not change the order of these fields
    // they are accessed from asm stubs via offsets
    // they might also be accessed from AngryCAT provided
    // machine code, which is not visible in stubs.s

    // zeroed before job execution
    void* result_buffer;                            // + 0x0
    UINT64 result_buffer_len;                       // + 0x8
    // undefined contents on job entry
    void* scratch_space;                            // + 0x10
    UINT64 scratch_space_len;                       // + 0x18
    void* current_machine_code_slot_address;        // + 0x20
    void* current_microcode_slot_address;           // + 0x28
    UINT64 core_id;                                 // + 0x30
    UINT64 ret_gpf_value;                           // + 0x38
    UINT64 ret_rdtsc_value;                         // + 0x40
    CoreFaultInfo* fault_info;                      // + 0x48
    // during init the RSP is stored here
    // after recovery RSP is restored to this value
    UINT64 recovery_rsp;                            // + 0x50

    // ASR call gate
    // see call_asr_for_index in asr.h for details
    // not strictly a call gate, everything runs in Ring 0 anyway
    void* asr_gate;                                 // + 0x58

    // add new fields for asm stubs above this line,
    // but not in the middle of existing fields

    // end asm stubs accessible region


    // do not access below fields from asm stubs, their order
    // might change depending on C code changes
    // if you need access to them from asm, move them
    // higher, but below existing asm fields

    // if any function pointer is != NULL it is
    // called on the AP in its main loop
    // do not call UEFI functions
    CoreFunctionCall core_functions[CORE_FUNCTION_SLOTS];

    // updated by core loop with current rdtsc value
    // may only be written from the spinning core
    // main core may only read this value
    UINT64 heartbeat;

    // used by the EFI API to handle events of this core
    // as we spin loop this should never signal ready
    EFI_EVENT core_event;

    // misc fields
    UINT64 current_microcode_len;
    // if we have to move the pointer due to alignment
    // we should keep the original one around
    CoreFaultInfo* original_fault_info_ptr;
    // also needs to be aligned
    UINT8* fault_handlers;
    UINT8* original_fault_handlers_ptr;

    // set on core 0
    // during start of the AP main loop this will
    // be loaded into IDTR via LIDT
    void* idt_base;
    UINT64 idt_limit;

    // updated on core start and when requested
    void* cr3_value;

    // ASR registry for this core
    ASREntry* asr_registry;

    // IBS handling stores state here
    IBSControl* ibs_control;

    // DSM handling stores state here
    DSMControl* dsm_control;


    // flags used to control the core state
    // read and written from core_id and core 0
    // even though these are 1 bit flags, they are
    // stored as 8 byte integers for easier atomic read/writes
    // wasteful, but simple

    // core is actually present on this CPU
    UINT64 present;
    // core was started and is in its main loop
    UINT64 started;
    // core has no active job and is ready to accept one
    UINT64 ready;
    // core 0 has written a new job for this core
    UINT64 job_queued;
    // use with lock_context and unlock_context
    // only C code will respect this
    // while a job is running, no locks should be acquired
    // and the job locks this structure
    // when starting a core, lock this context before
    // starting the actual loop function
    // the main loop will unlock this context after it's init
    volatile UINT64 lock;
} CoreContext;
#pragma pack(pop)

typedef struct JobParameters_s {
    UINT64 core_id;
    UINT64 machine_code_slot;
    UINT64 ucode_slot;
    UINT64 timeout;
    UINT64 options;
} JobParameters;

// how many core contexts are allocated
// not the amount of cores on the system
#define MAX_CORE_COUNT 128

extern CoreContext core_contexts[MAX_CORE_COUNT];

#endif /* CORES_H */
