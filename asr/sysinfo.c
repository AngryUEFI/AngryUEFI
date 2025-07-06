#include "sysinfo.h"

#include <Base.h>

#include "handlers/ucode.h"
#include "handlers/ucode_execute.h"

SMP_SAFE void* get_ucode_slot_addr(CoreContext* context, UINT64 slot_number) {
    if (slot_number >= UCODE_SLOTS) {
        return NULL;
    }

    return ucodes[slot_number].ucode;
}

SMP_SAFE void* get_machine_slot_addr(CoreContext* context, UINT64 slot_number) {
    if (slot_number >= MACHINE_CODE_SLOTS) {
        return NULL;
    }

    return machine_codes[slot_number].machine_code;
}
