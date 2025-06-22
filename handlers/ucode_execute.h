#ifndef UCODE_EXECUTE_H
#define UCODE_EXECUTE_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_apply_ucode_execute_test(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_execute_machine_code(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_send_machine_code(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_get_last_test_result(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

void init_ucode_execute();

// referenced in ASM accessible region in CoreContext
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

#endif /* UCODE_EXECUTE_H */
