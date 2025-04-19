#include "paging.h"

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "AngryUEFI.h"
#include "Protocol.h"

#include "system/paging_structures.h"
#include "system/paging_stubs.h"

// convert the given partial address (40 bits) into a full 64 bit address
// the least significant 12 bits are set to 0
// the most significant 12 bits are set to 0 
static UINT64 get_full_address(UINT64 addr) {
    UINT64 ret = 0x0;
    ret = ret | (addr << 12);
    return ret;
}

// shift out the least significant 12 bits
static UINT64 get_short_address(UINT64 addr) {
    return addr >> 12;
}

PML4e* get_pml4(void* cr3)
{
    return cr3;
}

PDPTe* get_pdpt_at(void* cr3, UINTN pml4_index) {
    if (pml4_index >= 512) {
        return NULL;
    }

    PML4e pml4e = get_pml4(cr3)[pml4_index];
    UINT64 partial_address = pml4e.bits.addr;
    UINT64 full_address = get_full_address(partial_address);

    return (PDPTe*)full_address;
}

PDe* get_pd_at(void* cr3, UINTN pml4_index, UINTN pdpt_index) {
    PDPTe* pdpt = get_pdpt_at(cr3, pml4_index);
    if (pdpt == NULL || pdpt_index >= 1024) {
        return NULL;
    }

    PDPTe pdpte = pdpt[pdpt_index];
    UINT64 partial_address = pdpte.bits.addr;
    UINT64 full_address = get_full_address(partial_address);

    return (PDe*)full_address;
}

PTe* get_pt_at(void* cr3, UINTN pml4_index, UINTN pdpt_index, UINTN pt_index) {
    PDe* pd = get_pd_at(cr3, pml4_index, pdpt_index);
    if (pd == NULL || pt_index >= 1024) {
        return NULL;
    }

    PDe pde = pd[pt_index];
    UINT64 partial_address = pde.bits.addr;
    UINT64 full_address = get_full_address(partial_address);

    return (PTe*)full_address;
}

UINT64 get_entry(void* cr3, void* address, UINTN* level) {
    return 0;
}

#define CURRENT_TABLE 0xffff
#define PREVIOUS_ENTRY 0xfffe
// 1280 + 3 * 8 = 1304 Bytes
// a bit below our max payload size
#define MAX_ENTRIES_PER_PACKET 80

void write_entry(UINT64* target_buffer, UINT64 entry, UINT16 position, UINT8 paging_level) {
    *(UINT16*)target_buffer = position;
    ((UINT8*) target_buffer)[2] = paging_level;
    target_buffer[1] = entry;
}

static EFI_STATUS send_pageinfo_message(UINT64* payload, UINT64 pageinfo_entries, BOOLEAN last_message, ConnectionContext* ctx) {
    EFI_STATUS status = EFI_SUCCESS;
    
    payload[2] = pageinfo_entries;
    const UINTN payload_size = pageinfo_entries * 16 + 3 * 8;
    status = construct_message(response_buffer, sizeof(response_buffer), MSG_PAGINGINFO, (UINT8*)payload, payload_size, last_message);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = send_message(response_buffer, payload_size + HEADER_SIZE, ctx);
    if (EFI_ERROR(status)) {
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS send_table(UINT64* table_base, UINT16 table_length, UINT8 paging_level, UINT64 flags, void* cr3_value, ConnectionContext* ctx) {
    EFI_STATUS status = EFI_SUCCESS;
    // write header into buffer
    UINT64* payload_u64 = (UINT64*)payload_buffer;
    payload_u64[0] = flags;
    payload_u64[1] = (UINT64)cr3_value;

    UINT64* current_payload_position = &payload_u64[3];
    for (UINTN i = 0; i < table_length; i++) {
        if (i % MAX_ENTRIES_PER_PACKET == 0 && i != 0) {
            status = send_pageinfo_message(payload_u64, MAX_ENTRIES_PER_PACKET, FALSE, ctx);
            if (status != EFI_SUCCESS) {
                FormatPrint(L"Unable to send pageinfo message: %r\n", status);
                send_status(0x101, FormatBuffer, ctx);
                return status;
            }

            // reset position in buffer
            current_payload_position = &payload_u64[3];
        }

        write_entry(current_payload_position, table_base[i], (UINT16)i, paging_level);
        current_payload_position += 2;
    }
    
    // this also works for no entries left in buffer, a message with no page entries will be send
    const UINTN entries_left_in_buffer = (current_payload_position - &payload_u64[3]) / 2;
    status = send_pageinfo_message(payload_u64, entries_left_in_buffer, TRUE, ctx);
    if (status != EFI_SUCCESS) {
        FormatPrint(L"Unable to send pageinfo message: %r\n", status);
        send_status(0x101, FormatBuffer, ctx);
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS send_entry(UINT64* entry, UINT16 position, UINT8 paging_level, UINT64 flags, void* cr3_value, ConnectionContext* ctx) {
    EFI_STATUS status = EFI_SUCCESS;
    // write header into buffer
    UINT64* payload_u64 = (UINT64*)payload_buffer;
    payload_u64[0] = flags;
    payload_u64[1] = (UINT64)cr3_value;

    write_entry(&payload_u64[3], *entry, position, paging_level);
    status = send_pageinfo_message(payload_u64, 1, TRUE, ctx);
    if (status != EFI_SUCCESS) {
        FormatPrint(L"Unable to send pageinfo message: %r\n", status);
        send_status(0x101, FormatBuffer, ctx);
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS handle_get_paging_info(UINT8* payload, UINTN payload_length, ConnectionContext* ctx) {
    PrintDebug(L"Handling MSG_GETPAGINGINFO message.\n");
    if (payload_length < 40) {
        FormatPrint(L"MSG_GETPAGINGINFO is too short, need at least 40 Bytes, got %u.\n", payload_length);
        send_status(0x1, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    UINT64* payload_u64 = (UINT64*)payload;
    const UINT64 core_id = payload_u64[0];
    const UINT64 pml4_index = payload_u64[1];
    const UINT64 pdpt_index = payload_u64[2];
    const UINT64 pd_index = payload_u64[3];
    const UINT64 pt_index = payload_u64[4];

    if (core_id >= MAX_CORE_COUNT) {
        FormatPrint(L"Core id %u is out of range, only max %u cores are supported.\n", core_id, MAX_CORE_COUNT);
        send_status(0x201, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    CoreContext* context = &core_contexts[core_id];
    if (context->present != 1) {
        FormatPrint(L"Core id %u is not present on this CPU.\n", core_id);
        send_status(0x202, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    if (context->started != 1) {
        FormatPrint(L"Core id %u is not started.\n", core_id);
        send_status(0x203, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    // TODO: update this on APs
    void* cr3 = context->cr3_value;
    // TODO: update this depending on update result, for now always fresh
    UINT64 flags = 0x1;

    PML4e* pml4 = get_pml4(cr3);
    if (pml4_index == CURRENT_TABLE) {
        return send_table((UINT64*)pml4, 512, 4, flags, cr3, ctx);
    } else if (pml4_index >= 512) {
        FormatPrint(L"PML4 index %u is out of range, max 511.\n", pml4_index);
        send_status(0x2, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    PDPTe* pdpt = get_pdpt_at(cr3, pml4_index);
    if (pdpt_index == CURRENT_TABLE) {
        return send_table((UINT64*)pdpt, 1024, 3, flags, cr3, ctx);
    } else if (pdpt_index == PREVIOUS_ENTRY) {
        return send_entry((UINT64*)&pml4[pml4_index], pml4_index, 4, flags, cr3, ctx);
    } else if (pdpt_index >= 1024) {
        FormatPrint(L"PDPT index %u is out of range, max 1023.\n", pdpt_index);
        send_status(0x3, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    PDe* pd = get_pd_at(cr3, pml4_index, pdpt_index);
    if (pd_index == CURRENT_TABLE) {
        return send_table((UINT64*)pd, 1024, 2, flags, cr3, ctx);
    } else if (pd_index == PREVIOUS_ENTRY) {
        return send_entry((UINT64*)&pdpt[pdpt_index], pdpt_index, 3, flags, cr3, ctx);
    } else if (pd_index >= 1024) {
        FormatPrint(L"PD index %u is out of range, max 1023.\n", pd_index);
        send_status(0x4, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    }

    PTe* pt = get_pt_at(cr3, pml4_index, pdpt_index, pd_index);
    if (pt_index == CURRENT_TABLE) {
        return send_table((UINT64*)pt, 1024, 1, flags, cr3, ctx);
    } else if (pt_index == PREVIOUS_ENTRY) {
        return send_entry((UINT64*)&pd[pd_index], pd_index, 2, flags, cr3, ctx);
    } else if (pt_index >= 1024) {
        FormatPrint(L"PT index %u is out of range, max 1023.\n", pt_index);
        send_status(0x5, FormatBuffer, ctx);
        return EFI_INVALID_PARAMETER;
    } else {
        return send_entry((UINT64*)&pt[pt_index], pt_index, 1, flags, cr3, ctx);
    }

    FormatPrint(L"Unreachable reached. PML4: %u, PDPT: %u, PD: %u, PT: %u, core: %u\n", pml4_index, pdpt_index, pd_index, pt_index, core_id);
    send_status(0x6, FormatBuffer, ctx);
    return EFI_INVALID_PARAMETER;
}

void* create_minimal_paging(void* base_cr3) {
    PML4e* pml4 = get_pml4(base_cr3);
    PML4e* pml4_new = AllocatePages(1);
    CopyMem(pml4_new, pml4, 512 * sizeof(PML4e));

    PDPTe* pdpt = get_pdpt_at(base_cr3, 0);
    PDPTe* pdpt_new = AllocatePages(2);
    CopyMem(pdpt_new, pdpt, 1024 * sizeof(PDPTe));

    PDe* pd = get_pd_at(base_cr3, 0, 0);
    PDe* pd_new = AllocatePages(2);
    CopyMem(pd_new, pd, 1024 * sizeof(PDe));

    // unmap all pages in PDPT[0], except first 4 GB
    for (UINTN i = 4; i < 1024; i++) {
        pdpt_new[i].bits.present = 0;
        pdpt_new[i].bits.ps = 0;
    }

    // update new PML4, first entry to point to our new pdpt
    pml4_new[0].bits.addr = get_short_address((UINT64)pdpt_new);
    // unmap the rest in the PML4
    for (UINTN i = 1; i < 512; i++) {
        pml4_new[i].bits.present = 0;
    }

    return pml4_new;
}