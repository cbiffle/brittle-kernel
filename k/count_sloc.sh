#!/bin/bash

# This is the way I generate lines-of-code metrics when reporting the TCB size.
# - All source files, including headers.
# - Excluding unit tests.
# - Excluding SoC-specific bridge code.

sloccount $(ls *.c *.cc *.h *.S | grep -v _test | grep -v stm32f4)
