#ifndef UCODE_EXECUTE_H
#define UCODE_EXECUTE_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_apply_ucode_execute_test(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_send_machine_code(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

void init_ucode_execute();

#pragma pack(push, 1)
typedef struct MachineCodeContainer_s {
    UINT8* machine_code;
    UINT64 length;
} MachineCodeContainer;
#pragma pack(pop)

// 0 - copied from ROM, see ../data/
// 1-9 - free use
#define MACHINE_CODE_SLOTS 10
extern MachineCodeContainer machine_codes[MACHINE_CODE_SLOTS];
#define MACHINE_CODE_SIZE 1*1024*1024

#define RESULT_BUFFER_SIZE 1*1024
#define MACHINE_CODE_SCRATCH_SPACE_SIZE 4*1024

#pragma pack(push,1)
typedef struct MachineCodeMetaData_s {
    void* result_buffer;
    UINT64 result_buffer_len;
    void* scratch_space;
    UINT64 scratch_space_len;
    void* current_machine_code_slot_address;
    void* current_microcode_slot_address;
    UINT64 core_id;
    UINT64 ret_gpf_value;
    UINT64 ret_rdtsc_value;
    UINT64 execution_done;
} MachineCodeMetaData;
#pragma pack(pop)

// how many core contexts are allocated
// not the amount of cores on the system
#define MAX_CORE_COUNT 128

#endif /* UCODE_EXECUTE_H */
