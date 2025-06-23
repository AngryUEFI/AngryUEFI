# Overview
Add new ucode updates that need to be compiled into AngryUEFI here. The naming convention is `description-CPUID.h`.

Only one update can be defined for each CPUID, if multiple updates are defined for a CPUID, it is undefined which one is chosen.

Read the Include Criteria below before adding new updates.

# Contents
The header files contain the following information:
* `unsigned char ucode_description_CPUID[]` - the actual data of the update, represented as a C array
* `unsigned int ucode_description_CPUID_len` - the length in byte of the update
* `unsigned int ucode_description_CPUID_cpuids[]` - matching CPUIDs for this update, terminate with a zero entry

# Creation
The script `AngryEDK2/binary-to-header.sh` can be used to create the C array and length. You will need to adjust the names of the variables, add const to all variables and add the matching CPUIDs. Call it like `./binary-to-header.sh <path_to_ucode_udpate> <path_to_output_file>`.

If an update matches multiple CPUIDs, use any of them for the name of the file and variables. Add all of them to `ucode_description_CPUID_cpuids`.

# Registering
After creating the file, the update needs to be added to `ucode_db.c`. Add the header file and array entry to `db_entries`.

# Background
AngryUEFI always provides a "known good" ucode update in ucode slot 0. This can optionally be applied by AngryUEFI after a test update was applied to a core. For this AngryUEFI needs to select the update based on the CPUID of the CPU it is running on.

# Selection
AngryUEFI scans the array `db_entries` for the first entry that contains a matching CPUID. If multiple entries match the same CPUID, the returned update is undefined.

The special value `0xffffffff` as CPUID means the update should be used as fallback, if no other update matches. This will likely not work if AngryCAT requests actually applying this fallback update, but allows AngryUEFI to start on unsupported CPUs. If multiple fallback updates are specified, it is undefined which one is selected.

Note: If needed, ucode slot 0 can be written from AngryCAT as well, allowing on the fly updates of a known good update or adding support for another CPUID without updating AngryUEFI. Changes are not persisted between AngryUEFI reboots.

# Inclusion criteria
Only updates without AMD's proprietary ucode will be accepted. In general, create template updates that zero all match registers and NOP all quads.

Updates in this DB should be no-op as much as feasible. Applying them should always succeed, assuming the ucode updater on the CPU core is still functional.

# Implementation notes
Do not include the update header files anywhere outside ucode_db.c The header files directly define data with non-static lifetime. If they are included multiple times, multiple definitions of the same symbol will occur.

This inclusion method blows up the compilation unit size of ucode_db.c massively and likely will not scale forever. The upside is that the individual updates do not need to be added to the build system.
