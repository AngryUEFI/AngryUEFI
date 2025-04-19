#ifndef PAGING_STUBS_H
#define PAGING_STUBS_H

#include <Base.h>

// for SMP_SAFE
#include "handlers/cores.h"

SMP_SAFE void set_cr3(void* new_cr3);
SMP_SAFE void* get_cr3();

#endif /* PAGING_STUBS_H */
