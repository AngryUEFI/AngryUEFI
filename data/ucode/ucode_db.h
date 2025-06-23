#ifndef UCODE_DB_H
#define UCODE_DB_H

#include <Base.h>

#include "AngryUEFI.h"

typedef struct UcodeDBEntry_s {
    const unsigned char* update;
    const unsigned int update_length;
    const unsigned int* matching_cpuids;
} UcodeDBEntry;

// can return NULL to indicate no update was found
// pointer is statically allocated, do not free
const UcodeDBEntry* get_update_for_cpuid(UINT32 cpuid);

#endif /* UCODE_DB_H */
