#include "ibs.h"

#include "stubs.h"

#define IBS_MSR_IBS_Execution_Control 0xC0011033

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
    return;
}

SMP_SAFE void init_ibs_on_core(CoreContext* contex) {
    if (!is_ibs_supported()) {
        return;
    }
    // disable IBS if it is enabled
    IBS_OP_CTL val;
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 0; // zero IbsOpEn bit -> disable micro op sampling
    val.bits.IbsOpCntCtl = 1; // count micro ops instead of round robin counter
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);
}

SMP_SAFE void* start_ibs(CoreContext* context) {
    IBS_OP_CTL val;
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 1; // set IbsOpEn bit -> enable micro op sampling
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
    val.val = read_msr_stub(IBS_MSR_IBS_Execution_Control);
    val.bits.IbsOpEn = 0;
    val.bits.IbsOpCntCtl = 1;
    val.bits.IbsOpVal = 0;
    write_msr_stub(IBS_MSR_IBS_Execution_Control, val.val);

    return NULL;
}

SMP_SAFE void* clear_start_with_ibs_offset(CoreContext* context, UINT64 ibs_offset) {
    clear_ibs(context);
    set_ibs_offset(context, ibs_offset);
    start_ibs(context);

    return NULL;
}
