#ifndef IBS_H
#define IBS_H

#include <Base.h>

#include "asr.h"

#define MAX_IBS_ENTRIES 16384 // 512 = 1KB payload for response

// IBS (instruction based sampling) realted ASRs and helpers
// namespace 0x10000
// you probably do not want to call this on Intel CPUs, here be dragons
// the init functions will not do anything if is_ibs_supported() is false

// clear the IBS registers to capture a new sample
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

// store the current IBS entry into the result buffer
// returns the free space left in the buffer
// if no space left returns 0 and does not write anything
SMP_SAFE void* store_ibs_entry(CoreContext* context);  // 0x10006

// clear the IBS capture buffer in AngryUEFI
// starts a new capture run
// returns the number of events that can be captured in the buffer
SMP_SAFE void* clear_ibs_results(CoreContext* context); // 0x10007

// clears any filters and the result buffer
SMP_SAFE void* clear_ibs_results_reset_filter(CoreContext* context); // 0x10008

// number of events stored in the current buffer
// will stop recording if the max amount is reached
SMP_SAFE void* get_current_ibs_event_count(CoreContext* context); // 0x10011

// maximum number of events that can be stored in the buffer
SMP_SAFE void* get_max_ibs_event_count(CoreContext* context); // 0x10012

// allows rough filtering of IBS events
// check defines below IBSEvent for what to set
SMP_SAFE void* set_ibs_event_filter(CoreContext* context, UINT64 mask); // 0x10013

// copies entry_count IBS entries starting at start_index to target_buffer
// each entry is 16 Bytes, make sure target_buffer is big enough
// will only copy current entries based on the current position
// returns the number of entries copied
// mostly useful for dumping entries into the result buffer for testing/debugging
SMP_SAFE void* copy_ibs_entries(CoreContext* context, UINT8* target_buffer, UINT64 start_index, UINT64 entry_count); // 0x10014

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

// represents a single IBS event
// bits are a bit weird to fit things into 2 quad words
// primary use case is storing the event type
// plus phys & virt addresses for load/store
// if needed other event types can be stuffed in there as well
// based on Zen 2 "Processor Programming Reference (PPR) for AMD Family 17h Model 71h, Revision B0 Processors"
typedef struct IBSEvent_s {
    union {
        UINT64 val;
        struct {
            // 56 bit MSR
            // lower 12 bits are the same as virtual
            // -> shift out lower 12 bits to make room for status flags
            // take the lower 12 bits from the virtual address, even if the virtual
            // address is marked as invalid
            UINT64 phys:44;
            // IbsOpMicrocode. 1=Tagged operation from microcode
            UINT8 microcode:1;
            // DataSrc: northbridge IBS request data source
            // 7 -> MMIO/Config/PCI/APIC
            UINT8 data_source:3;
            // IbsOpMemWidth: load/store size in byte
            UINT8 size:4;
            // IbsSwPf: software prefetch. 1=The op is a software prefetch.
            UINT8 prefetch:1;
            // IbsDcPhyAddrValid: data cache physical address valid.
            UINT8 phys_valid:1;
            // IbsDcLinAddrValid: data cache linear address valid.
            UINT8 linear_valid:1;
            // IbsDcUcMemAcc: UC memory access (uncachable memory)
            UINT8 uncachable:1;
            // IbsStOp: store op
            UINT8 store:1;
            // IbsLdOp: load op
            UINT8 load:1;
            // IbsOpVal: micro-op sample valid
            // if for some reason no data was captured the rest of the fields might be UD
            UINT8 valid:1;
            UINT8 reserved:5;
        } fields;
      } q1;
      // 64 bit MSR
      // if virt valid contains that register
      // if virt invalid, contains the upper 52 bits of that register
      // and the lower 12 bits of the phys addr MSR
      // strategic bit reserve: this is, according to the docs, a canonical address
      // -> the upper bits should always the same and could be shifted out
      // at that point it might be best to add another u64 to this struct
      UINT64 q2;
} IBSEvent;

// IBS event filters
// discards any events not matching the set filter
#define IBS_FILTER_ALL 0x0
#define IBS_FILTER_ALU 0x1
#define IBS_FILTER_LOAD 0x2
#define IBS_FILTER_STORE 0x3
// store if load OR store (or both)
#define IBS_FILTER_LOAD_STORE 0x4
// store if both load AND store
#define IBS_FILTER_UPDATE 0x5

// control structure for IBS
typedef struct IBSControl_s {
    IBSEvent* event_buffer;
    // index of the next free slot
    // if >= MAX_IBS_ENTRIES -> full
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

typedef union {
    UINT64 val;
    struct {
      UINT64 Reserved1       : 23;
      UINT64 IbsOpMicrocode  :  1;
      UINT64 IbsOpBrnFuse    :  1;
      UINT64 IbsRipInvalid   :  1;
      UINT64 IbsOpBrnRet     :  1;
      UINT64 IbsOpBrnMisp    :  1;
      UINT64 IbsOpBrnTaken   :  1;
      UINT64 IbsOpReturn     :  1;
      UINT64 Reserved2       :  2;
      UINT64 IbsTagToRetCtr  : 16;
      UINT64 IbsCompToRetCtr : 16;
    } bits;
} IBS_OP_DATA_REGISTER;

typedef union {
    UINT64 val;
    struct {
      UINT64 Reserved1    : 58;
      UINT64 CacheHitSt   :  1;
      UINT64 RmtNode      :  1;
      UINT64 Reserved2    :  1;
      UINT64 DataSrc      :  3;
    } bits;
} IBS_OP_DATA2_REGISTER;

typedef union {
    UINT64 val;
    struct {
      UINT64 IbsLdOp               : 1;
      UINT64 IbsStOp               : 1;
      UINT64 IbsDcL1tlbMiss        : 1;
      UINT64 IbsDcL2TlbMiss        : 1;
      UINT64 IbsDcL1TlbHit2M       : 1;
      UINT64 IbsDcL1TlbHit1G       : 1;
      UINT64 IbsDcL2tlbHit2M       : 1;
      UINT64 IbsDcMiss             : 1;
      UINT64 IbsDcMisAcc           : 1;
      UINT64 Reserved0             : 4;
      UINT64 IbsDcWcMemAcc         : 1;
      UINT64 IbsDcUcMemAcc         : 1;
      UINT64 IbsDcLockedOp         : 1;
      UINT64 DcMissNoMabAlloc      : 1;
      UINT64 IbsDcLinAddrValid     : 1;
      UINT64 IbsDcPhyAddrValid     : 1;
      UINT64 IbsDcL2TlbHit1G       : 1;
      UINT64 IbsL2Miss             : 1;
      UINT64 IbsSwPf               : 1;
      UINT64 IbsOpMemWidth         : 4;
      UINT64 IbsOpDcMissOpenMemReqs: 6;
      UINT64 IbsDcMissLat          : 16;
      UINT64 IbsTlbRefillLat       : 16;
    } bits;
} IBS_OP_DATA3_REGISTER;

typedef union {
    UINT32 val;
    struct {
      UINT32 Vector     :  8; // bits   7– 0: Interrupt vector number
      UINT32 MsgType    :  3; // bits  10– 8: Message type
      UINT32 Reserved1  :  1; // bit     11: Reserved (RAZ)
      UINT32 DS         :  1; // bit     12: Delivery Status
      UINT32 Reserved2  :  3; // bits 13–15: Reserved (RAZ)
      UINT32 Mask       :  1; // bit     16: Mask (0=Not masked, 1=Masked)
      UINT32 Reserved3  : 15; // bits 17–31: Reserved (RAZ)
    } bits;
} LVTEntry;

#pragma pack(pop)

#endif /* IBS_H */
