#include "asr.h"

#include <Base.h>

// ASR registry
// include your header here and add your ASR to the array
// last entry must be all zero
// keep list sorted by ASR number

#include "sysinfo.h"
#include "ibs.h"

ASREntry asr_registry[] = {
    {0x1001, get_ucode_slot_addr},
    {0x1002, get_machine_slot_addr},

    {0x10001, clear_ibs},
    {0x10002, set_ibs_offset},
    {0x10003, start_ibs},
    {0x10004, start_with_ibs_offset},
    {0x10005, clear_start_with_ibs_offset},
    {0x10006, store_ibs_entry},
    {0x10007, clear_ibs_results},
    {0x10008, clear_ibs_results_reset_filter},
    // {0x10011, get_current_ibs_event_count},
    // {0x10012, get_max_ibs_event_count},
    // {0x10013, set_ibs_event_filter},
    // zero entry to signal end of list
    {0, NULL}
};

// Note: for now this is a basic linear search
// we require the list to be sorted in case we want binary search later
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
