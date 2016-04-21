Architecture Rules
==================

The kernel is designed and implemented with the following rules in mind.

.. _rule1:

Rule 1 (keys).
  *All kernel authority, services, and resources flow from explicit
  capabilities, or "keys," which are transferrable, revocable, and
  unforgeable.*

  There is no ambient authority and no other in-kernel access control mechanism.
  The things a program can do to affect the system and other programs can be
  determined completely by inspecting the capabilities it holds.

  Merely holding a capability is not enough to perform any operation; the
  capability must be explicitly named when exercising the operation.  This
  helps to avoid confused deputy scenarios when a key is delivered to a
  third-party unexpectedly.

  This has a subtle but important implication: all application code must run in
  the processor's unprivileged mode, or it could subvert the capability system.

.. _rule2:

Rule 2 (minimality).
  *The privileged codebase should be minimal.*

  Kernel code runs in the processor's "privileged" mode, which makes it harder
  to isolate and reason about.  Thus, a bug in the kernel can have serious,
  subtle, and wide-ranging effects.  It is necessarily the most trusted of an
  embedded application's trusted codebases.

  In Brittle, we interpret minimality two different (but related) ways.  First,
  the set of abstractions and operations provided by the kernel should be as
  small as possible (analogous to Liedtke's Minimality Principle from L4).

  Second, the actual line count of the privileged codebase should be as low as
  possible.  This shall not be interpreted as permission to obfuscate the code;
  clarity is more important than brevity for brevity's sake.  However, the
  careful application of abstractions and refactoring can make code the *right*
  size for what it's describing.

.. _rule3:

Rule 3 (no implicit allocation).
  *All resource allocation is explicit and under system (not kernel) control.*

  The kernel does not, for example, allocate book-keeping structures from a
  kernel heap as a side effect of operations.  The memory layout of kernel
  objects, and their allocation/deallocation, is managed entirely by
  unprivileged code.

.. _rule4:

Rule 4 (predictable timing).
  *All kernel operations shall have predictable execution times.*

  More specifically, kernel operations are always constant-time with respect to
  any parameters that can vary at runtime.  The execution time cannot vary with
  e.g. the number of tasks waiting on a particular event.  (Some operations are
  linear in configuration parameters, such as the number of task priorities.)
  This enables Brittle to support hard real time systems and applications.

.. _rule5:

Rule 5 (proxying).
  *All kernel operations can be transparently proxied.  Primitives provided to
  the system and application should be designed with proxying in mind.*

  Applications must not be able to tell whether a capability they hold allows
  direct communication with the kernel, or whether it's being intercepted and
  emulated by the system.  This is critical to allow systems to implement their
  own security and access control policies, hardware abstraction layers, etc.
  It's also important for both virtualization and test.

  This rule has important and far-reaching implications for the design of the
  messaging mechanism.

.. _rule6:

Rule 6 (enough rope).
  *The kernel shall protect itself from the system, not protect the system from
  the system.*

  Brittle uses the hardware's isolation features to keep stray pointer writes
  (for example) in the system from breaking any kernel invariants.  This is
  important.

  But Brittle makes no attempt to keep the system from doing stupid things.  If
  the system would like to revoke all its own authority, or give a buggy driver
  ultimate power over all applications... be our guest.

  Put another way: *we assume you know what you're doing.*

Rules 1, 2, and 3 are common in third-generation microkernels such as seL4.
The rest are unusual, and have caused Brittle to look pretty different from its
peers.


What's In a Kernel?
-------------------

Our interpretation of the rules above have led to the following general scope
of the kernel.

In Scope
^^^^^^^^

Isolation support.
  Describing parts of the physical address space and modeling application access
  rights.  (Primarily through :ref:`Memory objects <memory-object>`.)

Multiprogramming (thread) support.
  Sharing the single physical CPU between multiple (potentially isolated)
  programs.  (Primarily through :ref:`Context objects <context-object>`.)

Interprocess communication support.
  Once we have isolated programs, we need a controlled way for them to
  communicate with one another when necessary.  (Through the IPC mechanism and
  :ref:`Gate objects <gate-object>`.)

Unprivileged driver support.
  The ARMv7-M architecture normally reserves some features important for
  writing drivers, particularly interrupt handling, for use by privileged code.
  The kernel provides mechanisms to expose these features to unprivileged
  system code.  (Primarily :ref:`Interrupt objects <interrupt-object>`.)

Kernel resource management.
  Facilities for unprivileged code to reason about and control kernel resource
  allocation and use.  (In particular, the :ref:`object-table`.)

Out Of Scope
^^^^^^^^^^^^

Two services often found in kernels --- even microkernels --- are missing in
Brittle.

Drivers.
  Applications can implement drivers in unprivileged code; the Brittle kernel
  doesn't typically have any SoC depenencies.

Time.
  The kernel has no notion of time, timeouts, or time-slices.  Applications can
  implement this as needed. [#notime]_


.. rubric:: Footnotes

.. [#notime] The kernel's ignorance of time appears to be unique among
   microkernels.  So that's interesting.
