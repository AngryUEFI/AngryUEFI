#ifndef IBS_H
#define IBS_H

#include <Base.h>

#include "asr.h"

#define MAX_IBS_ENTRIES 512 // = 1KB payload for response

// IBS (instruction based sampling) realted ASRs and helpers
// namespace 0x10000
// you probably do not want to call this on Intel CPUs, here be dragons
// the init functions will not do anything if is_ibs_supported() is false

// clear the IBS buffer and prepare the core for a new sample run
SMP_SAFE void* clear_ibs(CoreContext* context); // 0x10001

// set the target offset in the IBS sample
// use this to target a specific micro op
// it is a bit fuzzy how to target the correct op
// recommended to call this and the instruction under test in a loop
// hint: use current and max ibs event count to determine whether more events can be captured
SMP_SAFE void* set_ibs_offset(CoreContext* context, UINT64 ibs_offset); // 0x10002

// enable IBS, after this the micro op determined by the ibs offset will be logged
SMP_SAFE void* start_ibs(CoreContext* context); // 0x10003

// combines set_ibs_offset and start_ibs
SMP_SAFE void* start_with_ibs_offset(CoreContext* context, UINT64 ibs_offset); // 0x10004

// clear -> set offset -> start
SMP_SAFE void* clear_start_with_ibs_offset(CoreContext* context, UINT64 ibs_offset); // 0x10005

// number of events stored in the current buffer
// will stop recording if the max amount is reached
SMP_SAFE void* get_current_ibs_event_count(CoreContext* context); // 0x10011

// maximum number of events that can be stored in the buffer
SMP_SAFE void* get_max_ibs_event_count(CoreContext* context); // 0x10012

// allows rough filtering of IBS events
// check defines below IBSEvent for what to set
SMP_SAFE void* set_ibs_event_filter(CoreContext* context, UINT64 mask); // 0x10013

// support functions that are not actually ASRs

// must be called once from the boot core to init the context for IBS support
// allocates memory etc. that can not be done on an AP
void init_ibs_for_context(CoreContext* context);

// sets up core specific configs that must be done on the AP
// called during startup of the AP
// must be re-entrant to support recovery
SMP_SAFE void init_ibs_on_core(CoreContext* contex);

// return true if IBS is supported
// checks the CPUID entries
SMP_SAFE BOOLEAN is_ibs_supported();

// called from ISR to store the current IBS entry into the buffer
SMP_SAFE void store_ibs_entry(CoreContext* context);

// represents a single IBS trace event
// q1.type one of ALU, READ[1-16], WRITE[1-16]
// q1.low is the physical address for READ and WRITE
// q2 is the virtual address for READ and WRITE
typedef struct IBSEvent_s {
    union {
        UINT64 q1_all;
        struct {
          UINT64 low:56;
          UINT8  type:8;
        } q1_fields;
      } q1;
      UINT64 q2;
} IBSEvent;

// values for q1.type
#define IBS_EMPTY 0x00
#define IBS_ALU 0x01
#define IBS_UNKNOWN 0xFF

#define IBS_READ1 0x10
#define IBS_READ2 0x11
#define IBS_READ4 0x12
#define IBS_READ8 0x13
#define IBS_READ16 0x14

#define IBS_WRITE1 0x20
#define IBS_WRITE2 0x21
#define IBS_WRITE4 0x22
#define IBS_WRITE8 0x23
#define IBS_WRITE16 0x24

// IBS event filters
// discards any events not matching the set filter
#define IBS_FILTER_ALL 0x0
#define IBS_FILTER_ALU 0x1
#define IBS_FILTER_READ 0x2
#define IBS_FILTER_WRITE 0x3
#define IBS_FILTER_READ_WRITE 0x4

// control structure for IBS
typedef struct IBSControl_s {
    IBSEvent event_buffer;
    UINT64 current_position;
    UINT64 ibs_filter;
} IBSControl;

#pragma pack(push,1)

typedef union {
  UINT64 val;
  struct {
    UINT64 IbsOpMaxCntLow   : 16;
    UINT64 Reserved0        :  1;
    UINT64 IbsOpEn          :  1;
    UINT64 IbsOpVal         :  1;
    UINT64 IbsOpCntCtl      :  1;
    UINT64 IbsOpMaxCntHigh  :  7;
    UINT64 Reserved1        :  5;
    UINT64 IbsOpCurCnt      : 27;
    UINT64 Reserved2        :  5;
  } bits;
} IBS_OP_CTL;

#pragma pack(pop)

#endif /* IBS_H */
