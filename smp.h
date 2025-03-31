#ifndef SMP_H
#define SMP_H

#include <Base.h>

typedef VOID (EFIAPI *EFI_AP_PROCEDURE)(  IN OUT VOID  *Buffer);

EFI_STATUS init_smp();
UINTN get_available_cores();
/*
    * func_buffer is passed to func as its first and only argument
    * completion_event returns the completion even corresponding to the started function
    * timeout can be 0 for infinite runtime, hard locked functions (microcode loop) might not terminate
*/
EFI_STATUS start_on_core(UINTN core_index, EFI_AP_PROCEDURE func, void* func_buffer, EFI_EVENT* completion_event, UINTN timeout);
// returns EFI_SUCCESS for finished function
EFI_STATUS check_func_status(EFI_EVENT completion_event);
EFI_STATUS close_event(EFI_EVENT completion_event);


#endif /* SMP_H */
