#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
// #include <Library/TimerLib.h>

#include "utils.h"

// TODO: timerlib is a no-op that causes an INT 3
// find a way to have some kind of time counting

UINT64 get_tsc() {
    // return GetPerformanceCounter();
    return 0;
}

UINT64 get_ms_diff(UINT64 tsc1, UINT64 tsc2) {
    // UINT64 frequency = GetPerformanceCounterProperties(NULL, NULL);
    // UINT64 ms_diff = (tsc2 - tsc1) * 1000 / frequency;

    // return ms_diff;
    return 0;
}