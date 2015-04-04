#ifndef K_MM_FAULT_H
#define K_MM_FAULT_H

namespace k {

void * mm_fault(void * stack);

void mm_fault_k(void * vstack);

}  // namespace k

#endif  // K_MM_FAULT_H
