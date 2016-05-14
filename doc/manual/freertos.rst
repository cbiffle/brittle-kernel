Case Study: FreeRTOS
====================

.. warning:: This section is slightly out of date.


Introduction
------------

`FreeRTOS`_ is a popular open source embedded operating system that targets a
similar class of processors as the Brittle kernel.

Brittle and FreeRTOS have different perspectives on several important points.
For example, the current Brittle kernel does not support destruction of kernel
objects at runtime, and will likely never support in-kernel priority
inheritance [#pi]_.  Thus, one might reasonably question whether Brittle could
support the same sort of dynamic applications as FreeRTOS.

But in fact Brittle was designed for this sort of thing.  To show how, we have
ported FreeRTOS V9.0.0 [#fr9]_ to run on Brittle as a :ref:`System layer
<parts>`, along with some simple demo applications.  We'll refer to the
combined system as FreeRTOS/Brittle, by analogy to GNU/Linux.

.. _FreeRTOS: http://freertos.org/


About FreeRTOS/Brittle
----------------------

FreeRTOS/Brittle is not a FreeRTOS API emulation layer or simulator.  It is the
actual FreeRTOS code, derived from the ``ARM_CM3`` port, *including the
scheduler*.  This means that all FreeRTOS features are supported, and behave
identically to the native ARM Cortex-M3 FreeRTOS port, with a couple of notable
exceptions that will be highlighted in this chapter.

To be clear, the FreeRTOS System layer implements:

- Allocation and deletion of OS objects, such as tasks and queues, on the fly
  out of a System-layer heap.

- Mutexes with priority inheritance.

- A notion of time, including operation timeouts and time-slicing with
  preemption.

This serves as an existence proof that Brittle's abstractions are sufficient to
implement these features outside the kernel.

By running on top of the Brittle kernel, FreeRTOS gains several features that
are missing from the ``ARM_CM3`` port [#mpu]_:

- A memory-protected environment where access to peripherals, etc. must be
  whitelisted through explicit capabilities, to avoid bugs that access
  unexpected areas of the address space.

- The ability to run entirely in unprivileged code, which means that memory
  protection is always enforced --- even in interrupt handlers.

- The ability to run a *hybrid system* combining FreeRTOS drivers and native
  Brittle drivers, which benefit from further isolation and robustness
  guarantees.  Applications can gradually migrate drivers into isolated
  environments to incrementally improve robustness.


What It Is Not
^^^^^^^^^^^^^^

FreeRTOS/Brittle may not be suitable for all FreeRTOS applications.

- The port uses more resources: about 16kiB of additional ROM, and a few kiB of
  additional RAM, compared to the native port.

- Performance is somewhat reduced: interrupt response latency is up by about a
  factor of 10, in the current port, which also affects the speed of preemptive
  context switches.

- The port is missing some features: custom handling of ARMv7-M faults is not
  currently piped through into FreeRTOS.


Structure of the Port
---------------------

The Code (High Level)
^^^^^^^^^^^^^^^^^^^^^

FreeRTOS is separated into a portable core and a collection of system-specific
port layers that adapt the core to particular architectures.  This work is
modeled after the Cortex-M3 port, which in the FreeRTOS sources lives at the
following path:

    FreeRTOS/Source/portable/GCC/ARM_CM3

FreeRTOS/Brittle, in turn, lives here:

    FreeRTOS/Source/portable/GCC/ARM_CM3_brittle

Like the ``ARM_CM3`` port, FreeRTOS/Brittle consists of only two source files in
this directory, and *no changes* to FreeRTOS's portable core.

The ``ARM_CM3`` port uses privileged code, interrupts, and long stretches of
assembly to provide processor-specific hooks for the portable core.  It's 434
lines of code [#freertos-loc]_ .

FreeRTOS/Brittle does the same thing, but uses unprivileged code, kernel
objects, and messages.  It compares favorably to the native port at 537 lines of
code.  Interestingly, it is implemented almost entirely in straight C, with only
two explicit assembly language instructions.

.. note:: When comparing port sizes, note that FreeRTOS/Brittle does not support
  tickless idle, which would otherwise add a hundred or more lines of support
  code.  This support is not absent because of limitations in Brittle, but
  because the tickless idle support proved fragile, littered with assumptions,
  and hard to port.


The Approach
^^^^^^^^^^^^

.. note:: There are many possible ways of mapping FreeRTOS concepts onto Brittle
  concepts.  This is the particular one we used.

The current implementation of Brittle doesn't allow kernel objects to be
destroyed once they have been created.  FreeRTOS, on the other hand, can create
and destroy objects (like tasks) freely.

Instead of trying to map FreeRTOS objects to Brittle objects, then, we opted to
treat the Brittle kernel like a hypervisor, and use its abstractions to define a
*machine model* for FreeRTOS to target.


Contexts Model Execution Priority Levels
########################################

FreeRTOS/Brittle defines, at minimum, two :ref:`Contexts <kor-context>`:

- The Task Context models the processor's Thread execution mode and is used to
  run FreeRTOS task code.

- The Interrupt Context(s) model the processor's Handler execution mode, with
  one Context per priority level used.  These are used to run interrupt
  handlers for hardware interrupts (including the SysTick Timer), and also to
  implement some virtual interrupts described below.

.. note:: The current port uses a single Interrupt Context for simplicity, which
  is equivalent to setting all interrupts to the same priority.  We'll refer to
  the singular "Interrupt Context" for the rest of this chapter.


Messages Model Supervisor Calls
###############################

The Task and Interrupt Contexts share access to a Gate, called the System Gate.
Both Contexts are given branded keys to the System Gate during initialization;
the Task Context is given the right to send to it, and the Interrupt Context to
receive from it.

The FreeRTOS port layer sends Brittle IPC messages through the System Gate to
perform two operations --- the specific operations in the ``ARM_CM3`` port that
require privileged CPU instructions:

- Requesting a context switch (from the implementation of ``vTaskYield``,
  originally implemented by pending a PendSV exception).

- Enabling/disabling interrupts (also used by the FreeRTOS critical section
  code, originally implemented by altering ``BASEPRI``).

The Interrupt Context holds :ref:`Service Keys <kor-context-service-key>` to
both itself and the Task Context.  It uses the authority granted by those keys
to perform these "privileged" operations: it can rewrite its own ``BASEPRI``
directly, and it can rewrite the Task Context's ``BASEPRI`` while the Task
Context is blocked in IPC.


Context Switches Multiplex the Task Context
###########################################

Because the single Task Context is shared by all FreeRTOS task code, the
FreeRTOS scheduler treats it just like a processor's register set: when it's
time to preform a (FreeRTOS) context switch, the registers from the Context are
saved into the (FreeRTOS) task control block, and the registers from another
task control block are loaded in to replace them.

The Interrupt Context performs this "task swap" using the Service Key it holds
to the Task Context.


The Message Dispatch Loop Multiplexes the Interrupt Context
###########################################################

FreeRTOS/Brittle configures the Interrupt Context to run an infinite loop
receiving and handling IPC messages.  These messages come from two sources:

1. Code running on the Task Context, requesting the "system call" services
   described above.

2. Hardware interrupts, modeled with kernel :ref:`Interrupt objects
   <kor-interrupt>`, including the SysTick Timer.

The dispatch loop looks at two parts of the recieved message:

- The brand of the key used to send the message, which distinguishes messages
  from Interrupt objects from "syscall" messages from the Task Context.

- The message selector field, which distinguishes operations.

In response, it performs System operations, including calling application
interrupt handler routines.


Application Code Runs In Both Contexts
######################################

FreeRTOS allows application code to run in interrupt context --- both to
implement new drivers, and to add functionality to FreeRTOS's own interrupt
service routines, such as the one for the SysTick Timer.  This means application
code is running in the processor's privileged mode, where certain isolation
properties (particularly memory protection) are broken.

FreeRTOS/Brittle does something similar but safer: when needed, System code
running in the Interrupt Context will call out to application routines, either
to dispatch hardware interrupts, or to implement FreeRTOS interrupt hooks.

The most visible piece of application code that runs in the Interrupt Context is
not an interrupt handler, but the traditional entry point, ``main``.  The
``main`` routine in a FreeRTOS application is responsible for creating any
initial FreeRTOS objects that are required, and then starting the scheduler.

The AppInfo block in the FreeRTOS/Brittle image tells the kernel to start the
initial program in the Interrupt Context, not the Task Context, running
``main``.  The program's ``main`` eventually calls ``vTaskStartScheduler`` in
FreeRTOS, which kicks off the Interrupt Context message dispatch loop and yields
to the Task Context.

This has one interesting implication for the port.  FreeRTOS does not *expect*
its setup code to be called "from an interrupt."  In fact, many of the routines
applications need to use during setup are technically unsafe for use in
interrupts, such as ``xTaskCreate`` to create a task.  The most visible reason
why: they use the non-ISR version of critical sections under the hood.

But FreeRTOS/Brittle runs ``main`` from the equivalent of an interrupt, so we
have to lift this restriction.  The port does so by unifying the ISR and non-ISR
implementation of interrupt control, used by critical sections:

- The Interrupt Context holds its own Context Key in a particular key register,
  and can directly adjust its ``BASEPRI`` value.  Thus it acts somewhat
  privileged.

- The Task Context holds a *specially branded* key to the System Gate *in the
  same key register*.  The Interrupt Context's message dispatch loop recognizes
  this key's brand, and *emulates* the :ref:`Context protocol
  <context-methods>` for adjusting the ``BASEPRI`` register only.

Thus, FreeRTOS/Brittle code running on either context can adjust ``BASEPRI`` by
sending the *same bytes to the same key register*, but we didn't have to give
the Task Context its own key (with all the authority that would convey).


Discussion
----------

Things Shown
^^^^^^^^^^^^

**You can build dynamic applications on a static kernel.**  The port allocates
a fixed set of kernel objects before starting ``main``, and these objects live
for the duration of the application --- even when the application dynamically
creates and destroys tasks, mutexes, and the like.

**Brittle is usable from C.**  Brittle is implemented in C++11, but was designed
to be usable from legacy languages such as C and assembler.  As FreeRTOS/Brittle
is entirely C (and C90 at that), this seems to have worked.

**Expensive but important algorithms like priority inheritance can be
implemented outside the kernel.**  There are no constant-time algorithms for
general priority inheritance, of the sort required by FreeRTOS, and so priority
inheritance cannot be implemented inside the Brittle kernel.  FreeRTOS/Brittle
suggests that this might be okay.


Problems Encountered
^^^^^^^^^^^^^^^^^^^^

**Poor context-switch latency.**  The context switch latency between FreeRTOS
tasks is 10-50x slower in FreeRTOS/Brittle than in a native FreeRTOS port.  The
culprit: the relatively high cost of Brittle IPC in the current implementation.
The IPC path has not been optimized and can take 1000 cycles.  Because the
FreeRTOS scheduler is implemented outside the kernel and interacts with kernel
objects via IPC, every context switch generates around 20 IPCs.  We've been
able to reduce this by adding bulk operations to the Context protocol, such as
:ref:`context-method-read-registers`.  Once the design has stabilized, a
fast-path implementation for inter-Context IPC should be able to reduce the IPC
cost by a factor of 10, as shown by the similar fast-path implementations in
EROS, L4, and MINIX 3.


.. rubric:: Footnotes

.. [#fr9] Actually v9.0.0-rc2.  v9.0.0 was running a bit late.
.. [#pi] Brittle is unlikely to natively support priority inheritance because,
  so far, I have not been able to figure out how to implement priority
  inheritance using constant-time algorithms.  Brittle *is*, however, likely to
  provide primitives to make it easier to implement priority inheritance in the
  system.
.. [#mpu] There is an ``ARM_CM3_MPU`` port that provides a form of memory
  isolation, but because FreeRTOS lacks a messaging primitive for efficiently
  calling between isolated tasks, I've found that isolation terribly awkward to
  use.
.. [#freertos-loc] As with every other "lines of code" measurement in this
  report, this figure was generated using David A. Wheeler's SLOCCount tool.
