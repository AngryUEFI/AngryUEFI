#ifndef DSM_H
#define DSM_H

#include <Base.h>

#include "asr.h"

// prepare DSM internal state
// should be called at start of ASM stub
SMP_SAFE void* prepare_dsm(CoreContext* context);
// prepare DSM for capture
// should be called closely before starting the capture
SMP_SAFE void* prepare_dsm_capture(CoreContext* context);
// store events to DSM result buffer
// should be called after stopping the cature
SMP_SAFE void* store_dsm_capture(CoreContext* context);

// Recommended to use ASM directly to reduce amount of spurious events in buffer
// ASRs provided as reference
// start capturing events
SMP_SAFE void* start_dsm_capture(CoreContext* context);
// stop capturing events
SMP_SAFE void* stop_dsm_capture(CoreContext* context);

#endif /* DSM_H */
