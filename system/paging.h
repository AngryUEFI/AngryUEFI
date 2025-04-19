#ifndef PAGING_H
#define PAGING_H

#include <Base.h>

#include "AngryUEFI.h"
#include "system/paging_structures.h"

// Note:
// all functions assume identity paging
// no effort is made to translate the physical
// addresses to virtual ones

// 512 entries
PML4e* get_pml4(void* cr3);

// 1024 entries
PDPTe* get_pdpt_at(void* cr3, UINTN pml4_index);

// 1024 entries
PDe* get_pd_at(void* cr3, UINTN pml4_index, UINTN pdpt_index);

// 1024 entries
PTe* get_pt_at(void* cr3, UINTN pml4_index, UINTN pdpt_index, UINTN pt_index);

// returns the page entry for the given virtual address
// can return either a PTe, PDe or PDPTe
// check level for what was returned, PTe - 1, PDPTe - 3
UINT64 get_entry(void* cr3, void* address, UINTN* level);

EFI_STATUS handle_get_paging_info(UINT8* payload, UINTN payload_length, ConnectionContext* ctx);

// return a newly allocated pointer to the base of a new paging structure
// the structure contains pages for AngryUEFI, stack and heap
// uses the passed CR3 value to build the structure
void* create_minimal_paging(void* base_cr3);

#endif /* PAGING_H */
