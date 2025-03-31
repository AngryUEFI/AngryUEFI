#include "smp.h" // keep at the top to define EFI_AP_PROCEDURE, not defined in EDK2

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/MpService.h>
#include <Library/DebugLib.h>

#include "AngryUEFI.h"

static EFI_MP_SERVICES_PROTOCOL *MpServices = NULL;

EFI_STATUS init_smp() {
    EFI_STATUS Status;
    if (MpServices == NULL) {
        Status = gBS->LocateProtocol(&gEfiMpServiceProtocolGuid, NULL, (VOID **)&MpServices);
        if (EFI_ERROR(Status)) {
            FormatPrint(L"Failed to locate MP Services: %r\n", Status);
            return Status;
        }
    }

    return EFI_SUCCESS;
}

UINTN get_available_cores() {
    UINTN ProcessorCount, EnabledCount;
    EFI_STATUS Status;

    Status = MpServices->GetNumberOfProcessors(MpServices, &ProcessorCount, &EnabledCount);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"GetNumberOfProcessors failed: %r\n", Status);
        return 0;
    }

    return ProcessorCount;
}

EFI_STATUS start_on_core(UINTN core_index, EFI_AP_PROCEDURE func, void* func_buffer, EFI_EVENT* completion_event, UINTN timeout) {
    EFI_STATUS Status;

    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, DummyNotiftyFunction, NULL, completion_event);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Failed to create completion event: %r\n", Status);
        return Status;
    }

    Status = MpServices->StartupThisAP(
                 MpServices,
                 func,
                 core_index,
                 *completion_event,
                 timeout,
                 func_buffer,
                 NULL
             );

    if (EFI_ERROR(Status)) {
        FormatPrint(L"StartupThisAP failed on core %u: %r\n", core_index, Status);
    }
    return Status;
}


EFI_STATUS check_func_status(EFI_EVENT completion_event) {
    return gBS->CheckEvent(completion_event);
}

EFI_STATUS close_event(EFI_EVENT completion_event) {
    if (completion_event != NULL) {
        return gBS->CloseEvent(completion_event);
    }

    return EFI_SUCCESS;
}