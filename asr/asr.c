#include "asr.h"

#include <Base.h>

// ASR registry
// include your header here and add your ASR to the array
// last entry must be all zero

#include "sysinfo.h"

ASREntry asr_registry[] = {
    {0x1001, get_ucode_slot_addr},
    {0x1002, get_machine_slot_addr},
    // zero entry to signal end of list
    {0, NULL}
};

SMP_SAFE void* get_asr_for_index(CoreContext* context, UINT64 asr_index) {
    UINTN i = 0;
    ASREntry* current = &context->asr_registry[i];
    while (current->ptr != NULL) {
        if (current->index == asr_index) {
            return current->ptr;
        }
        current++;
    }

    return NULL;
}

SMP_SAFE ASREntry* get_default_asr_registry() {
    return asr_registry;
}
