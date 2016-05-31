/*
 * C Runtime startup for isolated programs loaded by the system.  The program
 * loader is responsible for much of what a conventional C startup routine
 * might do: filling in the GOT, initializing our data segment, setting the
 * stack pointer.  We just need to run any functions specified during
 * initialization.
 */

#include "etl/attribute_macros.h"

using InitFnPtr = void (*)();

extern "C" {
  void _start();

  extern InitFnPtr _preinit_array_start, _preinit_array_end;
  extern InitFnPtr _init_array_start, _init_array_end;

  void _init();
  void _init_epilogue();

  int main();
}

void _start() {
  for (auto * f = &_preinit_array_start; f != &_preinit_array_end; ++f) {
    (*f)();
  }

  _init();

  for (auto * f = &_init_array_start; f != &_init_array_end; ++f) {
    (*f)();
  }

  (void) main();
  while (true);
}

ETL_SECTION(".init_prologue")
ETL_NAKED
void _init() {
  asm volatile ("push {r4-r11, lr}");
}

ETL_SECTION(".init_epilogue")
ETL_NAKED
ETL_USED
void _init_epilogue() {
  asm volatile ("pop {r4-r11, pc}");
}

