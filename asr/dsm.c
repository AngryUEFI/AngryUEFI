#include "ibs.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include "stubs.h"


SMP_SAFE void* prepare_dsm(CoreContext* context) {
    return NULL;
}

SMP_SAFE void* prepare_dsm_capture(CoreContext* context) {
    return NULL;
}

SMP_SAFE void* store_dsm_capture(CoreContext* context) {
    return NULL;
}


SMP_SAFE void* start_dsm_capture(CoreContext* context) {
    return NULL;
}

SMP_SAFE void* stop_dsm_capture(CoreContext* context) {
    return NULL;
}
