#include "dsm.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include "dsm.h"


SMP_SAFE void* prepare_dsm(CoreContext* context) {
    enable_dsm_in_cr4_stub();

    write_fenced_fptan(0x03d0,0x80000000,0);
    write_fenced_fptan(0x03d2,0x1e,0);
    write_fenced_fptan(0x03d0,0x80001000,0);
    write_fenced_fptan(0x03d2,0xF24040000000001,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x110,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x100,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x110,0);
    UINT64 val = read_fenced_fpatan(0x22b,0,0);
    write_fenced_fptan(0x22b,val&(~0xFFFFFFFFFFFFFFFFUL),0);
    val = read_fenced_fpatan(0x22c,0,0);
    write_fenced_fptan(0x22c,val& (~0xFFFFUL),0);
    write_fenced_fptan(0x248,0x7017161211151413,0);
    write_fenced_fptan(0x249,0xde0aff01,0);
    write_fenced_fptan(0x03d0,0x80001032,0);
    write_fenced_fptan(0x03d2,0xfffffffffff0003f,0);
    write_fenced_fptan(0x03d0,0x0,0);

    return NULL;
}

SMP_SAFE void* prepare_dsm_capture(CoreContext* context) {
    DSM_file_header* filehdr = &context->dsm_control->file_header;
    ZeroMem(filehdr, sizeof(*filehdr));
    //file version hdr for python to read
    filehdr->version_info = (sizeof(*filehdr) << 32) | 0x2UL;

    write_fenced_fptan(0x03d0,0x00001032,0);
    UINT64 mask = read_fenced_fpatan(0x03d2,0,0);
    filehdr->mask = mask;

    UINT64 g1 = read_fenced_fpatan(0x22b,0,0);
    UINT64 c1 = read_fenced_fpatan(0x22c,0,0);
    UINT64 g2 = read_fenced_fpatan(0x248,0,0);
    UINT64 c2 = read_fenced_fpatan(0x249,0,0);

    for (UINT64 idx = 0; idx < 8; idx++) {
        if ((c2 & 1) &&  (c2 & (0x01UL<<(idx+8)) )) {
            filehdr->types |= 0x01UL<<(idx*8);
            filehdr->sel |= g2 & 0xFFUL<<(idx*8);
        } else if ((c1 & 1) &&  (c1 & (0x01UL<<(idx+8)) )) {
            filehdr->types |= 0x02UL<<(idx*8);
            filehdr->sel |= g1 & 0xFFUL<<(idx*8);
        }
    }

    for(int zzz = 0; zzz < 256; ++zzz) {
        CpuPause();
    }

    // clear out our capture results storage
    for (UINT64 i = 0; i < MAXIMUM_DSM_ENTRIES; i++) {
        DSMEvent* event = &context->dsm_control->event_buffer[i];
        event->data0 = 0;
        event->data1 = 0;
    }
    CpuPause();

    write_fenced_fptan(0x03d0,0x00001000,0);
    context->dsm_control->orig = read_fenced_fpatan(0x03d2,0,0);
    write_fenced_fptan(0x03d0,0x80001000,0);
    write_fenced_fptan(0x03d2,context->dsm_control->orig & ~(1UL<<50),0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x110,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x100,0);

    for (UINT64 index = 0; index < MAXIMUM_DSM_ENTRIES; index++) {
        write_fenced_fptan(0x03d0,0x80003001,0);
        write_fenced_fptan(0x03d2,0x80000000|index,0);
        write_fenced_fptan(0x03d0,0x80003003,0);
        write_fenced_fptan(0x03d2,0,0);
        write_fenced_fptan(0x03d0,0x80003002,0);
        write_fenced_fptan(0x03d2,0,0);
    }

    __asm__ __volatile__ ("mfence"   : : : "memory");
    write_fenced_fptan(0x03d0,0x80003001,0);
    write_fenced_fptan(0x03d2,0x0,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x110,0);
    write_fenced_fptan(0x03d0,0x80003000,0);
    write_fenced_fptan(0x03d2,0x100,0);

    return (void*)context->dsm_control->orig;
}

SMP_SAFE void* store_dsm_capture(CoreContext* context) {
    DSM_file_header* filehdr = &context->dsm_control->file_header;
    for(UINTN zzz = 0; zzz < 256; ++zzz) {
        CpuPause();
    }

    CpuPause();
    __asm__ __volatile__ ("mfence"   : : : "memory");
    
    write_fenced_fptan(0x03d0,0x00003000,0);
    UINT64 index_offset = read_fenced_fpatan(0x03d2,0,0);
    filehdr->idx_info = index_offset;
    index_offset = (index_offset>>48)&(MAXIMUM_DSM_ENTRIES-1);

    for (UINT64 index = 0; index < MAXIMUM_DSM_ENTRIES; index++) {
        write_fenced_fptan(0x03d0,0x80003001,0);
        write_fenced_fptan(0x03d2,(index_offset + index )&(MAXIMUM_DSM_ENTRIES-1),0);
        UINT64 data0;
        UINT64 data1;
        write_fenced_fptan(0x03d0,0x00003003,0);
        data0 = read_fenced_fpatan(0x03d2,0,0);
        write_fenced_fptan(0x03d0,0x00003002,0);
        data1 = read_fenced_fpatan(0x03d2,0,0);
        DSMEvent* event = &context->dsm_control->event_buffer[index];
        event->data0 = data0;
        event->data1 = data1;
    }

    CpuPause();
    __asm__ __volatile__ ("mfence"   : : : "memory");

    write_fenced_fptan(0x03d0,0x80001000,0);
    write_fenced_fptan(0x03d2,context->dsm_control->orig,0);
    write_fenced_fptan(0x03d0,0x00001000,0);
    CpuPause();
    __asm__ __volatile__ ("mfence"   : : : "memory");

    filehdr->num_items = MAXIMUM_DSM_ENTRIES;

    return NULL;
}

SMP_SAFE void* get_start_stop_stub_asm_address(CoreContext* context) {
    return (void*)write_fenced_fprem_asm;
}

SMP_SAFE void* start_dsm_capture(CoreContext* context) {
    write_fenced_fprem(0x80001000,context->dsm_control->orig,0);
    return NULL;
}

SMP_SAFE void* stop_dsm_capture(CoreContext* context) {
    write_fenced_fprem(0x80001000,context->dsm_control->orig& ~(1UL<<50),0);
    return NULL;
}


void init_dsm_for_context(CoreContext* context) {
    context->dsm_control = AllocateZeroPool(sizeof(DSMControl));
    const UINTN alloc_size = ROUND_UP(sizeof(DSMEvent) * MAXIMUM_DSM_ENTRIES, 4096);
    context->dsm_control->event_buffer = AllocatePages(alloc_size / 4096);
}

SMP_SAFE void init_dsm_on_core(CoreContext* contex) {

}

SMP_SAFE void* copy_dsm_entries(CoreContext* context, UINT8* target_buffer, UINT64 start_index, UINT64 entry_count) {
    DSMControl* status = context->dsm_control;
    if (status->file_header.num_items <= start_index) {
        return 0;
    }

    UINT64 end_index = MIN(start_index + entry_count, status->file_header.num_items);
    for (UINT64 i = start_index; i < end_index; i++) {
        CopyMem(target_buffer, &status->event_buffer[i], sizeof(DSMEvent));
        target_buffer += sizeof(DSMEvent);
    }

    return (void*)(end_index - start_index);
}
