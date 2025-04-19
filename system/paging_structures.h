#ifndef PAGING_STRUCTURES_H
#define PAGING_STRUCTURES_H

#include <Base.h>

/*  
 * A single PML4 entry (PML4E) in x86-64 long mode.
 * Total size: 8 bytes.
 */
typedef union {
    UINT64 value;
    struct {
        UINT64 present        : 1;  // P:      Page present in memory
        UINT64 rw             : 1;  // R/W:    Read/write (0 = read-only, 1 = read/write)
        UINT64 us             : 1;  // U/S:    User/supervisor (0 = supervisor, 1 = user)
        UINT64 pwt            : 1;  // PWT:    Page-level write-through
        UINT64 pcd            : 1;  // PCD:    Page-level cache-disable
        UINT64 accessed       : 1;  // A:      Accessed (set by CPU)
        UINT64 ignored1       : 1;  // Ignored by hardware
        UINT64 must_be_zero   : 1;  // Must be zero in long mode
        UINT64 avl            : 4;  // AVL:    Available to software
        UINT64 addr           :40;  // Address of next-level table (bits 12–51)
        UINT64 reserved       :11;  // Reserved, must be zero
        UINT64 nx             : 1;  // NX:     No-execute
    } __attribute__((packed)) bits;
} PML4e;

/*
 * Page‑Directory‑Pointer‑Table Entry (PDPTE) — points to a PD or
 * (if PS=1) directly maps a 1 GiB page.
 */
typedef union {
    UINT64 value;
    struct {
        UINT64 present      : 1;  // P
        UINT64 rw           : 1;  // R/W
        UINT64 us           : 1;  // U/S
        UINT64 pwt          : 1;  // PWT
        UINT64 pcd          : 1;  // PCD
        UINT64 accessed     : 1;  // A
        UINT64 ignored1     : 1;  // Must be zero (reserved)
        UINT64 ps           : 1;  // PS: 0=points to PD, 1=1 GiB page
        UINT64 avl          : 4;  // Available to software
        UINT64 addr         :40;  // Physical addr of PD or 1 GiB page (bits 12–51)
        UINT64 reserved     :11;  // Must be zero
        UINT64 nx           : 1;  // NX
    } __attribute__((packed)) bits;
} PDPTe;

/*
 * Page‑Directory Entry (PDE) — points to a PT or
 * (if PS=1) directly maps a 2 MiB page.
 */
typedef union {
    UINT64 value;
    struct {
        UINT64 present      : 1;  // P
        UINT64 rw           : 1;  // R/W
        UINT64 us           : 1;  // U/S
        UINT64 pwt          : 1;  // PWT
        UINT64 pcd          : 1;  // PCD
        UINT64 accessed     : 1;  // A
        UINT64 dirty        : 1;  // D
        UINT64 ps           : 1;  // PS: 0=points to PT, 1=2 MiB page
        UINT64 avl          : 4;  // Available to software
        UINT64 addr         :40;  // Physical addr of PT or 2 MiB page (bits 12–51)
        UINT64 reserved     :11;  // Must be zero
        UINT64 nx           : 1;  // NX
    } __attribute__((packed)) bits;
} PDe;

/*
 * Page‑Table Entry (PTE) — maps a 4 KiB page.
 */
typedef union {
    UINT64 value;
    struct {
        UINT64 present      : 1;  // P
        UINT64 rw           : 1;  // R/W
        UINT64 us           : 1;  // U/S
        UINT64 pwt          : 1;  // PWT
        UINT64 pcd          : 1;  // PCD
        UINT64 accessed     : 1;  // A
        UINT64 dirty        : 1;  // D
        UINT64 pat          : 1;  // PAT
        UINT64 global       : 1;  // G
        UINT64 avl          : 3;  // Available to software
        UINT64 addr         :40;  // Physical base of 4 KiB page (bits 12–51)
        UINT64 reserved     :11;  // Must be zero
        UINT64 nx           : 1;  // NX
    } __attribute__((packed)) bits;
} PTe;

#endif /* PAGING_STRUCTURES_H */
