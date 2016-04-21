System Programmer's Model
=========================

Introduction
------------

This section is aimed at people who want to understand Brittle's model and
APIs, and particularly people interested in implementing a *system layer*
between the kernel and applications.


Kernel Design Principles
------------------------

The kernel is designed and implemented with the following rules in mind.

.. _rule1:

Rule 1 (keys).
  *All kernel authority and services flow from explicit capabilities, or
  "keys," which are transferrable, revocable, and unforgeable.*

  There is no ambient authority and no other in-kernel access control mechanism.
  The things a program can do to affect the system and other programs can be
  determined completely by inspecting the capabilities it holds.

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
  kernel heap as a side effect of operations.  (In the current version,
  resources are entirely allocated at startup, a sort of degenerate
  satisfaction of this rule.)

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
  emulated by the system.  This is critical for both virtualization and test,
  and has important implications for the design of interprocess communication
  mechanisms.

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

In Scope
^^^^^^^^

The kernel's facilities fall into the following general areas.

Isolation support.
  Describing parts of the physical address space and modeling application access
  rights.

Multiprogramming (thread) support.
  Sharing the single physical CPU between multiple (potentially isolated)
  programs.

Interprocess communication support.
  Once we have isolated programs, we need a controlled way for them to
  communicate with one another when necessary.

Unprivileged driver support.
  The ARMv7-M architecture normally reserves some features important for
  writing drivers, particularly interrupt handling, for use by privileged code.
  The kernel provides mechanisms to expose these features to unprivileged
  system code.

Kernel resource management.
  Reflecting over kernel objects and operating on them.  (Eventually:
  allocation.)

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


Keys and Objects
----------------

Programs (system or application) access kernel services using capabilities
(:ref:`Rule 1 (keys) <rule1>`).  We follow the convention from KeyKOS and EROS
and call them *keys*, because it's much easier to type.  Each key designates a
single kernel *object*, such as a task or an interrupt, and contains
information on which operations can be performed on that object using this
particular key (:ref:`key-branding`).

Each program running on the kernel has its own copy of the hardware processor
registers (``r0`` through ``r15`` and a couple others).  The kernel *extends*
the processor's register set with a number of *key registers*.  The contents of
these key registers are kept by the kernel on behalf of the program, in
isolated memory, so the program can neither inspect their contents nor attempt
to gain authority by overwriting ("forging") a key.

The kernel lets programs refer to their own key registers by number, and
manipulate their contents in two ways:

1. By copying the contents of one register to another (a sort of key version of
   the ``mov`` instruction), and

2. By performing an IPC operation on the object referenced by a key (a special
   version of the ``svc`` instruction).

These two operations are provided as syscalls, and represent the *entire*
program interface of the Brittle kernel.  All kernel facilities are exposed as
objects to programs holding the right keys, and all in-kernel operations ---
from changing permissions on an area of memory to killing a process --- are
modeled as IPC operations.  Besides being a pleasant design, this uniformity is
critical for :ref:`Rule 5 (proxying) <rule5>`.


Kinds of IPC Operations
^^^^^^^^^^^^^^^^^^^^^^^

Brittle uses an L4-style synchronous rendezvous IPC operation, in both blocking and non-blocking flavors.

Logically, every IPC operation consists of two optional phases:

Send phase.
  A message is transferred from the sender (caller) to the target.

Receive phase.
  A message is transferred back, from some remote object to the caller.

Since both phases are optional, it's entirely possible (but odd) to construct a do-nothing IPC operation that neither sends nor receives.

When *both* phases are enabled, the transition from send to receive is atomic.
This is most often used to implement "call-and-return" IPC operations, where
the sender delivers a request and then blocks waiting for the response.
However, the source for the received message need not be the same as the target
for the sent message, so this is also used in server loops to implement
"respond and wait for next request."

A particular common variety of IPC is the *call* operation.  It implements the
"call-and-return" pattern mentioned above, but issues to the target a
*single-use reply key*.  As its name implies, this key can be used *exactly
once* to deliver a result back to the caller.  Reply keys are implemented using
:ref:`key-revocation`.



Kinds of Kernel Objects
^^^^^^^^^^^^^^^^^^^^^^^

The kernel implements the following types of objects.

Context.
  An abstraction of the CPU state, which can be used to create threads or
  processes.

Address Range.
  A section of physical address space with associated access control data.
  Keys to an Address Range can specify a subset of its space, and are the
  building block for constructing isolated processes.

Gate.
  A special object that allows unprivileged code, running in a Context, to
  emulate other objects --- either existing kernel objects or entirely new,
  application-specific objects.

Object Table.
  An object that can reflect over the universe of kernel objects, including
  itself.  The Object Table has the sole ability to produce arbitrary keys, and
  also the power to revoke them atomically.

Interrupt.
  Represents a hardware interrupt and translates it into a message send.

Null.
  An object with no special features.  When a key is revoked (see
  :ref:`key-revocation`) it becomes a key to the null object.


.. _key-branding:

Key Branding
^^^^^^^^^^^^

In addition to referencing a kernel object, each key contains a data field,
called its *brand*.

Broadly, brands are used to distinguish keys, or families of keys, from each
other.  A key's brand is indelible: it is chosen when the key is initially
created, and cannot be altered after that.  If a key is copied or transferred,
the new copy will bear the exact same brand.

Brands allow for *key-based polymorphism*: an object may behave differently,
depending on which key is used to access it.  Access control is the simplest
version of this; many kernel objects will only allow certain operations when
accessed through a key with a particular brand bit set.

Address Range objects use a more complicated form of polymorphism.  An Address
Range describes an arbitrary chunk of memory, but *keys* to the Address Range
describe power-of-two-aligned sub-ranges with particular access control and
cache settings.  From the system's perspective there appear to be a nearly
infinite number of Address Range objects --- but since they are distinguished
only by data held in the key, the kernel doesn't need to allocate resources to
keep track of them, and typically hosts only a handful of real Address Ranges.

The most general use of brands is with Gates.  Gates are IPC endpoints used by
Contexts to communicate with each other.  In the simplest model, there are some
number of client Contexts sending to the gate (and waiting for replies), and a
single server Context receiving from the gate (and sending replies).

When one Context sends a message through a Gate to a second Context, the kernel
delivers the sender's Gate key brand to the reciever without interpreting it.
The receiver can impose any meaning on the brand it likes: different brands
might distinguish one client from another, contain access control information,
etc.


.. _key-revocation:

Key Revocation
^^^^^^^^^^^^^^

Brittle supports atomic, constant-time *revocation* of keys to kernel objects,
at the object level.  When an object's keys are revoked, they all atomically
become keys to the Null Object.

To ensure that revocation completes in constant time, regardless of the number
of extant keys, the actual cost is amortized over future uses of revoked keys.
Every kernel object has an associated *generation number*, and each key
contains a copy of it.  Whenever a key is used for IPC, the generation numbers
are compared; if they are different, the kernel replaces the key with a key to
the Null Object.  Thus revoking all the keys to an object is as simple as
incrementing its generation number.

Revocation is used internally to implement single-use reply keys for call-style
IPC.  It is also available to the system, through the Object Table's Invalidate
method.

.. attention::

   Generation numbers for normal keys are currently 32 bits, and so they *can*
   wrap around, causing previously revoked keys to become valid again (known as
   "key resurrection").  Systems that make heavy use of revocation, or that
   expect to be running for long periods, may need to provide supplemental
   mechanisms for scavenging revoked but not yet nulled keys.  We intend to
   provide support for this but it isn't yet implemented.

   Note that this problem affects reply keys at a lower rate, because reply
   keys use the brand to extend the generation number to 96 bits.


Scheduling and Priority
-----------------------

At any given time, a Brittle-based application is either running unprivileged
application code in a Context (the *current Context*), or is running privileged
kernel code to process an application request.  The kernel is strictly
event-based and does not contain an idle loop or any long-running processes, so
applications must ensure that at least one Context is always *runnable* so that
the kernel can return into it.


The Kernel Scheduler
^^^^^^^^^^^^^^^^^^^^

Like most L4-influenced kernels, Brittle uses a simple fixed-priority
round-robin scheduler.

In the event that more than one Context is runnable, the Context's *priority*
values are compared to determine which gets control.  Brittle maintains the
following priority invariant when choosing which Context to run:

.. _priority-invariant:

Priority Invariant.
  *When an unprivileged instruction executes, it is either: (1) the next
  instruction specified by the highest-priority runnable Context, when context
  switching is not suppressed, or (2) the next instruction in the current
  Context if context switching is suppressed.*

Scheduling *within* a priority level is round-robin: if a Context blocks on an
operation (such as an IPC) and is later made runnable again, it goes to the end
of the queue for its priority level.

The number of priority levels is configurable when the kernel is built, and
determines both the size of some kernel data structures, and the time cost of
searching for runnable Contexts in certain cases.

The scheduler is "fixed-priority" in that the kernel never changes the priority
of a Context on its own.  However, the priority of a Context can be changed on
the fly by the *system*, using Context's Set Priority method.  Per the priority
invariant, changing the priority of a Context may cause a context switch.  This
can be used to implement priority inheritance or priority ceiling schemes.


Suppressing Context Switches
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As suggested by the wording of the :ref:`priority invariant
<priority-invariant>`, the kernel scheduler can effectively be "turned off."
It's possible (but unexplored) to write cooperatively scheduled applications on
Brittle.

In Brittle, truly preemptive context switches only occur in response to
interrupts (possibly including a timer interrupt, if the system provides such a
thing).  When an interrupt arrives, it may unblock a Context containing an
interrupt handler; if that Context's priority is higher than that of the
current Context, the kernel normally triggers a context switch before executing
further unprivileged instructions (per the priority invariant).

These context switches can be disabled on a per-Context basis, either
temporarily or permanently.  Brittle virtualizes the hardware's ``BASEPRI``
register, giving each Context its own copy.  This means that the system can
configure a Context to be insensitive to interrupts below a particular priority
level.

.. note::

  While each Context has its own copy of ``BASEPRI``, it can't directly read or
  write it --- that could allow a buggy task to block preemption and take down
  the application.  ``BASEPRI`` must be accessed indirectly, through the kernel,
  using a Context key and the Read/Write Register methods.  The expectation is
  that *systems* will hold Context keys, but most Contexts won't hold their
  own.  That being said, if you'd like to allow your Contexts to alter their
  own ``BASEPRI`` values, nothing is stopping you from issuing them a key to
  themselves.  See :ref:`Rule 6 (enough rope) <rule6>`.

.. note::

  Context switches may also occur if the current Context explicitly *blocks
  itself* by sending a blocking IPC.  These cannot be prevented in the current
  system: if you don't want context switches, don't ask for them; again, see
  :ref:`Rule 6 (enough rope) <rule6>`.

Syscall ABI
-----------

IPC Exchange (sysnum 0)
^^^^^^^^^^^^^^^^^^^^^^^

IPC revolves around messages.  A message currently consists of:

- Four 32-bit words of data, one of which controls the IPC, and

- Four keys, one of which is the reply key.

This is pretty small, and might be enlarged later.  So far it's proven
adequate.  Remember that the message size is only really important for
interactions between the system and the kernel.  For interactions between the
application and the system, applications could send *pointers*, and the system
could use its authority to read arguments out of application memory.

In the :ref:`kernel-object-reference`, the four words are referred to
abstractly by the names ``d0`` through ``d3``, and the four keys, by ``k0``
through ``k3``.


IPC Descriptor Encoding
#######################

IPC operations are controlled by a *descriptor* word loaded into logical
register ``d0`` (physical register ``r4``).  The result of the operation is
also described using a descriptor, returned in the same register.

.. list-table:: Descriptor Bit Fields
  :header-rows: 1

  * - Hi
    - Lo
    - Name
    - On Entry
    - On Return
  * - 31
    - 28
    - Sysnum
    - 0 selects IPC operation
    - --
  * - 27
    - 24
    - Source
    - Key register index of source key for receive phase.
    - --
  * - 23
    - 20
    - Target
    - Key register index of target key for send phase.
    - --
  * - 19
    - 19
    - Block
    - If 1, caller is willing to block in first phase.
    - --
  * - 18
    - 18
    - Receive
    - If 1, enables receive phase.
    - --
  * - 17
    - 17
    - Send
    - If 1, enables send phase.
    - --
  * - 16
    - 16
    - Error
    - Signals error to receiver.
    - Indicates error in operation.
  * - 15
    - 0
    - Selector
    - Varies
    - Varies

The *selector* is used by kernel objects to indicate the method being called.
Applications can use it for the same thing, or as an additional 16 bits of
uninterpreted data.

A single-use reply key is issued only for specific *call* descriptors:

- Both send and receive phase enabled.
- Source key register for receive is zero.

This has the kind of odd effect that keys held in ``k0`` can't be used to
receive client messages, but as ``k0`` through ``k3`` are destroyed on any IPC
anyway, this seemed an acceptable tradeoff.

Register Usage on Entering Send Phase
#####################################

.. list-table::
  :header-rows: 1

  * - Logical
    - Physical
  * - ``d0`` (descriptor)
    - ``r4``
  * - ``d1``
    - ``r5``
  * - ``d2``
    - ``r6``
  * - ``d3``
    - ``r7``

Other registers are ignored.


Register Usage on Entering Receive Phase Without Send
#####################################################

.. list-table::
  :header-rows: 1

  * - Logical
    - Physical
  * - ``d0`` (descriptor)
    - ``r4``

Other registers are ignored.


Register Usage After Receive Phase
##################################

.. list-table::
  :header-rows: 1

  * - Logical
    - Physical
  * - ``d0`` (result descriptor)
    - ``r4``
  * - ``d1``
    - ``r5``
  * - ``d2``
    - ``r6``
  * - ``d3``
    - ``r7``
  * - ``brand[31:0]``
    - ``r8``
  * - ``brand[63:32]``
    - ``r9``

Other registers are undisturbed.


Register Usage After Send Without Receive
#########################################

.. list-table::
  :header-rows: 1

  * - Logical
    - Physical
  * - ``d0`` (result descriptor)
    - ``r4``
  * - ``d1``
    - ``r5``
  * - ``d2``
    - ``r6``
  * - ``d3``
    - ``r7``

In the event of an exception during send, the logical registers ``d1`` through
``d3`` contain exception information --- so even though we're not receiving a
message *per se* we can still receive a message in the form of an exception.

Other registers are undisturbed.


Copy Key (sysnum 1)
^^^^^^^^^^^^^^^^^^^

IPC operations deposit keys, including reply keys, into specific key registers.  If we want to keep these keys, we need to move them aside before issuing a second IPC.  Thus, the act of moving a key aside *cannot itself be an IPC.*

Thus, Brittle supports a second syscall, Copy Key.  Copy Key reads one of the
current context's key registers, and deposits a bitwise copy into a second key
register.

Copy Key doesn't affect *any* general-purpose registers, and only requires that a descriptor be loaded into ``r4``.


Copy Key Descriptor Encoding
############################

.. list-table:: Descriptor Bit Fields
  :header-rows: 1

  * - Hi
    - Lo
    - Name
    - On Entry
  * - 31
    - 28
    - Sysnum
    - 1 selects Copy Key
  * - 27
    - 24
    - Source
    - Key register index of source for copy.
  * - 23
    - 20
    - Target
    - Key register index of target for copy.
  * - 19
    - 0
    - --
    - Ignored, should be zero.



.. rubric:: Footnotes

.. [#notime] The kernel's ignorance of time appears to be unique among
   microkernels.  So that's interesting.
