#ifndef CORES_H
#define CORES_H

#include <Base.h>

#include "AngryUEFI.h"

void init_cores();

#define RESULT_BUFFER_SIZE 1*1024
#define MACHINE_CODE_SCRATCH_SPACE_SIZE 4*1024

typedef struct CoreContext_s CoreContext;

EFI_STATUS handle_start_core(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_core_status(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_core_count(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

EFI_STATUS send_core_status(CoreContext* context, BOOLEAN is_last_message, ConnectionContext* ctx);

// lock the context
// will spin until lock is available
// yes, this can deadlock your code
void lock_context(CoreContext* context);
void unlock_context(CoreContext* context);
// try to get a lock, fail if context is already locked
BOOLEAN try_lock_context(CoreContext* context);

typedef VOID (*JobFunction)(CoreContext* argument);
typedef VOID (*PrepareFunction)(CoreContext* argument);

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
    void* result_buffer; // + 0
    UINT64 result_buffer_len; // + 8
    // undefined contents on job entry
    void* scratch_space; // + 16
    UINT64 scratch_space_len; // + 24
    void* current_machine_code_slot_address; // + 32
    void* current_microcode_slot_address; // + 40
    UINT64 core_id; // + 48
    UINT64 ret_gpf_value; // + 56
    UINT64 ret_rdtsc_value; // + 64

    // add new fields for asm stubs above this line,
    // but not in the middle of existing fields

    // end asm stubs accessible region


    // do not access below fields from asm stubs, their order
    // might change depending on C code changes
    // if you need access to them from asm, move them
    // higher, but below existing asm fields
    
    // the actual job function to execute
    // assume this is running on an AP
    // this means no UEFI functions may be called
    // called via function that updates the flags below
    JobFunction job_function;
    void* job_function_argument;

    // if not NULL, called before the actual job function
    // use case is to perform additional processing
    // before going into asm stubs
    // mostly used to fill core cache with ucode update
    PrepareFunction prepare_function;
    void* prepare_function_argument;

    // updated by core loop with current rdtsc value
    // may only be written from the spinning core
    // main core may only read this value
    UINT64 heartbeat;

    // used by the EFI API to handle events of this core
    // as we spin loop this should never signal ready
    EFI_EVENT core_event;

    // misc fields
    UINT64 current_microcode_len;

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
