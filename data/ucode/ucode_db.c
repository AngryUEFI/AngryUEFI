#include "ucode_db.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi.h>

#include "AngryUEFI.h"

// include new DB entries here
#include "template-0x00A20F12.h"
#include "template-0x00A60F12.h"
#include "template-0x00870F10.h"

// add new DB entries to this array
UcodeDBEntry db_entries[] = {
    {ucode_template_0x00A20F12, ucode_template_0x00A20F12_len, ucode_template_0x00A20F12_cpuids},
    {ucode_template_0x00A60F12, ucode_template_0x00A60F12_len, ucode_template_0x00A60F12_cpudids},
    {ucode_template_0x00870F10, ucode_template_0x00870F10_len, ucode_template_0x00870F10_cpuids},
};

// for easier inclusion, the DB entries use unsigned int instead of a defined width data type
// bail out if this does not match our expectations
STATIC_ASSERT(sizeof(unsigned int) == 4);

#define FALLBACK_CPUID 0xffffffff

static BOOLEAN cpudid_matches(UcodeDBEntry* entry, UINT32 cpuid) {
    const unsigned int* current = entry->matching_cpuids;
    while (*current != 0) {
        if ((UINT32)(*current) == cpuid) {
            return TRUE;
        }
        current++;
    }

    return FALSE;
}

const UcodeDBEntry* get_update_for_cpuid(UINT32 cpuid) {
    const UcodeDBEntry* fallback = NULL;
    for (UINTN i = 0; i < sizeof(db_entries) / sizeof(*db_entries); i++) {
        UcodeDBEntry* current = &db_entries[i];
        if (cpudid_matches(current, FALLBACK_CPUID)) {
            fallback = current;
        }
        if (cpudid_matches(current, cpuid)) {
            return current;
        }
    }
    return fallback;
}
