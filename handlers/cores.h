#ifndef CORES_H
#define CORES_H

#include <Base.h>

#include "AngryUEFI.h"

void init_cores();

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

extern MachineCodeMetaData machine_code_meta_data[MAX_CORE_COUNT];

#endif /* CORES_H */
