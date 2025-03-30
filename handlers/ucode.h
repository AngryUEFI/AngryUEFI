#ifndef UCODE_H
#define UCODE_H

#include <Base.h>

#include "AngryUEFI.h"

void init_ucode();

EFI_STATUS handle_send_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_flip_bits(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_apply_ucode(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_apply_ucode_execute_test(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_read_msr(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);
EFI_STATUS handle_send_machine_code(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

#pragma pack(push, 1)
typedef struct UcodeContainer_s {
    UINT8* ucode;
    UINT64 length;
} UcodeContainer;
#pragma pack(pop)

// 0 - known good
// 1 - target of mutation operations
// 2-9 - free use
#define UCODE_SLOTS 10
extern UcodeContainer ucodes[UCODE_SLOTS];

// referenced in stub.s
extern UINT8* original_ucode;

// big enough for family 0x19/Zen 5
#define UCODE_SIZE 5568

#pragma pack(push, 1)
typedef struct MachineCodeContainer_s {
    UINT8* machine_code;
    UINT64 length;
} MachineCodeContainer;
#pragma pack(pop)

// 0 - hard coded
// 1-9 - free use
#define MACHINE_CODE_SLOTS 10
extern MachineCodeContainer machine_codes[MACHINE_CODE_SLOTS];
#define MACHINE_CODE_SIZE 1*1024*1024

#define RESULT_BUFFER_SIZE 1*1024
extern UINT8 result_buffer[RESULT_BUFFER_SIZE];

#define MACHINE_CODE_SCRATCH_SPACE_SIZE 4*1024
extern UINT8 machine_code_scratch_space[MACHINE_CODE_SCRATCH_SPACE_SIZE];

#endif /* UCODE_H */
