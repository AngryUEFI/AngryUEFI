#include "cores.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "smp.h"

MachineCodeMetaData machine_code_meta_data[MAX_CORE_COUNT] = {0};

static void allocate_core_contexts() {
    for (UINTN i = 0; i < MAX_CORE_COUNT; i++) {
        MachineCodeMetaData* current = &machine_code_meta_data[i];
        current->result_buffer = AllocateZeroPool(RESULT_BUFFER_SIZE);
        current->result_buffer_len = RESULT_BUFFER_SIZE;
        current->scratch_space = AllocateZeroPool(MACHINE_CODE_SCRATCH_SPACE_SIZE);
        current->scratch_space_len = MACHINE_CODE_SCRATCH_SPACE_SIZE;
        current->core_id = i;
    }
}

void init_cores() {
    allocate_core_contexts();
}