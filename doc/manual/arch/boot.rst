.. _boot:

Boot Process and Initial Environment
====================================

When the processor starts up, it executes the kernel's reset handler.  This
routine sets up kernel state and then begins booting the application.

The kernel is intended to be compiled separately from the application, and the
same kernel binary can be used for several different applications.  This means
the kernel binary needs to be informed, somehow of the application's initial
requirements and entry point.  This information is conveyed through a block of
ROM called ``AppInfo``.

The kernel is linked to expect the ``AppInfo`` block at a particular address ---
typically at the end of the kernel's ROM segment, but this is configurable at
build time.  The ``AppInfo`` block is fully specified by the
``common/app_info.h`` file in the kernel sources, but at a high level, it
contains the following information:

- A magic number used to detect kernel-application version mismatches, or empty
  ROM where the application should be.

- The application's initial program counter and stack pointer.

- The number of extra object table slots the application expects to need.

- The number of external interrupts the application plans to service.

- The number of :ref:`Memory objects <memory-object>` needed to describe the
  application's use of address space, with their locations and sizes.

- An additional section of memory, donated to the kernel to set up the
  application.

The kernel processes ``AppInfo`` and sets up

1. The :ref:`Object Table <object-table>`.

2. A table mapping hardware interrupt requests to :ref:`Interrupt objects
   <interrupt-object>`.

3. An initial :ref:`Context <context-object>` and :ref:`Reply Gate
   <reply-gate-object>`.

The first four slots in the Object Table are always occupied by four
*well-known objects*, created at this time:

===== ============================================
Index Object
===== ============================================
0     The :ref:`kor-null` object.
1     The :ref:`kor-object-table`.
2     The initial :ref:`kor-context`.
3     The initial Context's :ref:`kor-reply-gate`.
===== ============================================

Starting at index 4 are the Memory objects requested in the ``AppInfo`` block.

All remaining slots in the Object Table are initialized to contain
:ref:`kor-slot` object placeholders.

The initial Context is configured to begin executing code at the application's
initial program counter.  Its key registers are initially null, save for ``k4``,
which is seeded by the kernel with a key to the :ref:`Object Table
<object-table>`.  This gives the program a mechanism for generating any other
authority it requires.
