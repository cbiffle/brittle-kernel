#ifndef K_MM_FAULT_H
#define K_MM_FAULT_H

/*
 * C++ service routines for memory management fault handling.  These are
 * called from the handler in entry.S.
 */

namespace k {

// Fault in application.
void * mm_fault(void * stack);

// Fault in kernel.
void mm_fault_k(void * vstack);

}  // namespace k

#endif  // K_MM_FAULT_H
