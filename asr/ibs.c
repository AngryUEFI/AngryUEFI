#include "ibs.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include "stubs.h"

#define IBS_MSR_IBS_Execution_Control 0xC0011033
#define IBS_MSR_OP_DATA 0xC0011035
#define IBS_MSR_OP_DATA2 0xC0011036
#define IBS_MSR_OP_DATA3 0xC0011037
#define IBS_MSR_DC_Linear_Address 0xC0011038
#define IBS_MSR_DC_Physical_Address 0xC0011039

#define IBS_CPUID_LEAF 0x8000000

// "cAMD" from "AuthenticAMD"
// might break with weird CPUs
#define AMD_VENDOR_CHECK 0x444D4163

SMP_SAFE BOOLEAN is_ibs_supported() {
    if (call_cpuid_ecx(0) != AMD_VENDOR_CHECK) {
        return FALSE;
    }
    UINT32 val = call_cpuid_ecx(IBS_CPUID_LEAF);
    return (val & (1 << 10)) != 0;
}

void init_ibs_for_context(CoreContext* context) {
    if (!is_ibs_supported()) {
        return;
    }

    context->ibs_control = AllocateZeroPool(sizeof(IBSControl));
    context->ibs_control->event_buffer = AllocateZeroPool(MAX_IBS_ENTRIES * sizeof(IBSEvent));
}

SMP_SAFE void init_ibs_on_core(CoreContext* context) {
    if (!is_ibs_supported()) {
        return;
    }
    // disable IBS if it is enabled
    IBS_OP_CTL val;
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 0; // zero IbsOpEn bit -> disable micro op sampling
    val.bits.IbsOpCntCtl = 1; // count micro ops instead of round robin counter
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);

    // clear the IBS filter
    context->ibs_control->ibs_filter = IBS_FILTER_ALL;
}

SMP_SAFE void* start_ibs(CoreContext* context) {
    IBS_OP_CTL val;
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 1; // set IbsOpEn bit -> enable micro op sampling
    val.bits.IbsOpCntCtl = 1; // count micro ops instead of round robin counter
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);

    return NULL;
}

SMP_SAFE void* set_ibs_offset(CoreContext* context, UINT64 ibs_offset) {
    IBS_OP_CTL val;
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    UINT64 max_count = 1 << 20;
    max_count >>= 4; // actual value is multiplied by 16 internally
    // this targets the micro op specified by ibs_offset
    // the capture happens, if max_count == current_count
    UINT64 current_count = (1 << 20) - ibs_offset;

    val.bits.IbsOpCurCnt = current_count;
    val.bits.IbsOpMaxCntLow = max_count & 0xFFFF;
    val.bits.IbsOpMaxCntHigh = (max_count >> 16) & 0x7f; // 7 bit bitfield

    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);

    return NULL;
}

SMP_SAFE void* start_with_ibs_offset(CoreContext* context, UINT64 ibs_offset) {
    set_ibs_offset(context, ibs_offset);
    start_ibs(context);

    return NULL;
}

SMP_SAFE void* clear_ibs(CoreContext* context) {
    IBS_OP_CTL val;

    // disable IBS and mark current contents as invalid
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 0;
    // write twice to ensure IBS is disabled before marking the result as invalid
    // that way we do not start IBS again accidentially
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpCntCtl = 1;
    val.bits.IbsOpVal = 0;
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);

    // zero phys address
    write_msr_stub(IBS_MSR_DC_Physical_Address, 0x0);

    // zero virtual/linear address
    write_msr_stub(IBS_MSR_DC_Linear_Address, 0x0);

    return NULL;
}

SMP_SAFE void* clear_start_with_ibs_offset(CoreContext* context, UINT64 ibs_offset) {
    clear_ibs(context);
    set_ibs_offset(context, ibs_offset);
    start_ibs(context);

    return NULL;
}

SMP_SAFE void* clear_ibs_results(CoreContext* context) {
    context->ibs_control->current_position = 0;
    return (void*)MAX_IBS_ENTRIES;
}

SMP_SAFE void* clear_ibs_results_reset_filter(CoreContext* context) {
    context->ibs_control->ibs_filter = IBS_FILTER_ALL;
    return clear_ibs_results(context);
}

SMP_SAFE static BOOLEAN should_store(CoreContext* context, IBSEvent* event) {
    UINT64 filter = context->ibs_control->ibs_filter;
    switch (filter) {
        case IBS_FILTER_ALL:
            return TRUE;
        case IBS_FILTER_ALU:
            return event->q1.fields.load == 0 && event->q1.fields.store == 0;
        case IBS_FILTER_LOAD:
            return event->q1.fields.load == 1 && event->q1.fields.store == 0;
        case IBS_FILTER_STORE:
            return event->q1.fields.load == 0 && event->q1.fields.store == 1; 
        case IBS_FILTER_LOAD_STORE:
            return event->q1.fields.load == 1 || event->q1.fields.store == 1;
        case IBS_FILTER_UPDATE:
            return event->q1.fields.load == 1 && event->q1.fields.store == 1;
        default:
            return TRUE;
    }
}

SMP_SAFE static void fill_from_data3(IBSEvent* event, IBS_OP_DATA3_REGISTER data3) {
    event->q1.fields.load = data3.bits.IbsLdOp;
    event->q1.fields.store = data3.bits.IbsStOp;
    event->q1.fields.uncachable = data3.bits.IbsDcUcMemAcc;
    event->q1.fields.linear_valid = data3.bits.IbsDcLinAddrValid;
    event->q1.fields.phys_valid = data3.bits.IbsDcPhyAddrValid;
    event->q1.fields.prefetch = data3.bits.IbsSwPf;
    event->q1.fields.size = data3.bits.IbsOpMemWidth;
}

SMP_SAFE static void fill_from_control(IBSEvent* event, IBS_OP_CTL control) {
    event->q1.fields.valid = control.bits.IbsOpVal;
}

SMP_SAFE static void fill_from_data1(IBSEvent* event, IBS_OP_DATA_REGISTER data1) {
    event->q1.fields.microcode = data1.bits.IbsOpMicrocode;
}

SMP_SAFE static void fill_from_data2(IBSEvent* event, IBS_OP_DATA2_REGISTER data2) {
    event->q1.fields.data_source = data2.bits.DataSrc;
}

SMP_SAFE void* store_ibs_entry(CoreContext* context) {
    IBSEvent event = {0};
    IBS_OP_CTL control;
    IBS_OP_DATA_REGISTER data1;
    IBS_OP_DATA2_REGISTER data2;
    IBS_OP_DATA3_REGISTER data3;

    // skip processing if buffer is full anyway
    if (context->ibs_control->current_position >= MAX_IBS_ENTRIES) {
        return 0;
    }

    control.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    data3.val = read_msr_stub(IBS_MSR_OP_DATA3);

    fill_from_data3(&event, data3);

    // skip the rest of the MSRs if we don't store this anyway
    if (!should_store(context, &event)) {
        return (void*) (MAX_IBS_ENTRIES - context->ibs_control->current_position);
    }

    fill_from_control(&event, control);

    data1.val = read_msr_stub(IBS_MSR_OP_DATA);
    data2.val = read_msr_stub(IBS_MSR_OP_DATA2);
    fill_from_data1(&event, data1);
    fill_from_data2(&event, data2);

    UINT64 phys_addr = read_msr_stub(IBS_MSR_DC_Physical_Address);
    UINT64 lin_addr = read_msr_stub(IBS_MSR_DC_Linear_Address);

    // shift out low 12 bits, stored in Linear Address
    event.q1.fields.phys = phys_addr >> 12;

    event.q2 = lin_addr;
    if (event.q1.fields.linear_valid == 0) {
        // the linear address is invalid, store the low 12 bits of the phys address
        // in its lower 12 bits
        // the other bits are kept from the MSR, in case they are interesting
        UINT64 field = lin_addr;
        
        // zero lower 12 bits
        field >>= 12;
        field <<= 12;

        // set lower 12 bits to lower 12 bits of phys addr
        field |= phys_addr & 0xfff;
        
        event.q2 = field;
    }

    context->ibs_control->event_buffer[context->ibs_control->current_position] = event;
    context->ibs_control->current_position++;

    // return how much space is left in the buffer
    return (void*) (MAX_IBS_ENTRIES - context->ibs_control->current_position);
}
