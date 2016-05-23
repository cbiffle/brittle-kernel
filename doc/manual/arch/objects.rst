A Tour of the Kernel Objects
============================

This section gives a brief introduction to each type of object implemented in
the current version of the kernel, and some idea of how they relate and
interact.

Descriptions of objects and their operations here are informal but essentially
correct.  See the :ref:`kor` for pedantic details.

.. _context-object:

Contexts
--------

The first object a program encounters is a *Context*, because Contexts are used
to represent programs themselves.  The :ref:`boot process <boot>` sets up one
Context at startup, to run the application's entry point.

More specifically, a Context is a virtual representation of the processor's
unprivileged (Thread) execution mode.  It provides storage for all the
unprivileged processor state, so that it can be "backed up" when stopping a
program to process interrupts or implement multitasking.

The program whose state is stored in a Context is said to be "running in" or
"inhabiting" the Context.

Contexts extend the hardware processor model in two ways.

First, they add two sets of *virtual registers*, both of which are used to hold
keys.

- The MPU Region Registers virtualize the hardware's MPU registers, so that each
  Context effectively gets its own copy of the MPU.  These registers either hold
  keys to :ref:`memory-object`, to allow a program running in the Context to
  access memory or peripherals, or :ref:`kor-null` to leave a region unused.

- The Key Registers hold keys of any type for use with the IPC operation
  (below).

Second, Context extends the processor model with two *virtual instructions*, or
syscalls:

- *Copy Key* copies a key from one Key Register to another --- similar to how the
  ``mov`` instruction operates on normal registers.

- *IPC* transmits a message from the program to some other object, designated by
  a key in one of the Key Registers.

It's important to recognize that a program's authority is *entirely* embodied in
the contents of its Context's virtual registers (MPU Region and Key).
 
- A program cannot operate on, refer to, or even demonstrate the existence of,
  any object except those whose keys are held in the Key Registers of its
  Context.

- A program cannot read, write, or execute a single byte of memory except those
  permitted by the keys loaded in its Context's MPU Region Registers.

Because of this, a Context isn't much use without something to load into its
MPU Region Registers... which brings us to our next object.

.. note:: For more information, see the :ref:`kor-context` entry in the
  :ref:`kor`.


.. _memory-object:

Memory
------

*Memory* objects represent regions of the physical address space.  These may be
RAM, ROM, peripherals, or even unmapped space that would fault if accessed.
There is an initial set of Memory objects created at boot; from there, new
objects can only be derived from existing ones, not created whole-cloth.

Keys to Memory contain data describing what sorts of accesses can be performed
using that key.  It's thus possible to create both read-write and read-only keys
to the same Memory --- perhaps keeping the read-write key for yourself and
handing the read-only keys out to clients.

Memory objects form a hierarchy.  Programs can create *child* Memory that has
access to a subset of an existing Memory object (its *parent*).  This provides
an easy way to isolate a program or grant a server access to part of the
client's address space.

Programs can also divide Memory objects, destructively, into pieces.  This is
how programs pay the kernel for any new objects they wish to create --- for
example, additional Contexts to implement multitasking.  Programs whittle
Memory down to the size required for the desired object, and then send the
Memory a :ref:`"become" <memory-method-become>` message that donates the memory
and transforms it into a different kernel object.

Programs can read and write the insides of Memory objects freely (if the key
allows) by sending :ref:`"peek" <memory-method-peek>` and :ref:`"poke"
<memory-method-poke>` messages, or by loading the key into a Context's MPU
region registers for direct access using load and store instructions.  Either
way, once Memory is donated to the kernel and becomes a different type of
object, access is atomically revoked to protect
kernel state.

.. note:: For more information, see the :ref:`kor-memory` entry in the
  :ref:`kor`.

.. _gate-object:

Gates
-----

*Gates* serve as IPC rendezvous points for programs running in Contexts.  Gates
implement no native operations.  Instead, sending a message using a Gate key
will pass the message *through* the gate to any program waiting on the other
side.  (Programs can wait on a Gate by using the *receive* flavor of IPC
operation on a Gate key.)

If no program is waiting to receive the message, the program sending the message
can optionally block.  This puts the program's Context into a sleeping state
until some other program is ready to receive the message.  Alternatively, the
sender can opt not to block, and the message is discarded.

This style of messaging is called *synchronous rendezvous*, and means that Gates
themselves don't need to provide any storage for messages: messages are directly
conveyed *through* Gates from sender to recipient.

.. note:: For more information, see the :ref:`kor-gate` entry in the
  :ref:`kor`.


.. _interrupt-object:

Interrupts
----------

The zoo of objects described thus far is enough to implement multi-process
programs with memory isolation, using polling to detect hardware events.  But
polling can be expensive; it's sometimes better to put a program to sleep
waiting for a hardware event, using an interrupt.  Brittle virtualizes hardware
interrupts in an object called (predictably) *Interrupt*.  More importantly,
Brittle converts hardware interrupts into messages.

Each Interrupt object is associated with a single hardware interrupt request
line.  When the interrupt occurs, the Interrupt object *sends a message* to a
Gate, containing information about which interrupt fired.

A program can receive this message, react to it, and decide when (and if) to
re-enable the Interrupt for another round.

.. note::

  Brittle's Interrupt object models both NVIC-routed external interrupts, and
  the SysTick Timer exception.  It cannot be used to intercept architectural
  faults or exceptions such as Hard Fault.

Interrupts can be configured to send a message to any Gate, or reconfigured on
the fly, by passing a key via the "set target" operation.

.. note:: For more information, see the :ref:`kor-interrupt` entry in the
  :ref:`kor`.


.. _object-table:

The Object Table
----------------

The *Object Table* is a singleton which provides programs with a facility for
enumerating all kernel objects.

.. note::

  The Object Table is a particularly unusual aspect of Brittle's design that is
  likely to surprise readers familiar with other third-generation microkernels.
  If you're familiar with (say) seL4 and are skimming the docs, now would be a
  good time to stop skimming.

The Object Table presents itself as a fixed-size table (size chosen at build
time) consisting of *slots*.  Each slot is either *empty* or refers to a kernel
object of the types listed above.

Programs can hold keys to empty slots (represented as :ref:`kor-slot` objects).
They form a second currency, alongside Memory objects: a key to an empty slot
represents the right to increase the number of living objects, and is required
to split a Memory object in half.

Programs can also hold keys *to the Object Table itself*.  With a key to the
Object Table, a program can make its own rules:

- It can "mint" a key to any kernel object out of thin air.

- It can "inspect" the contents of a key to determine whether the key refers to
  a native kernel object, or a program through a Gate.

- It can "invalidate" an object, causing all existing keys to be immediately and
  atomically revoked.

These powers are intended for programs that implement the system layer atop the
kernel.  The assumption is that such programs will hold Object Table keys
closely, and not hand them out to less trusted programs.  However, this is not
enforced, because *it doesn't need to be* --- holding an Object Table key still
doesn't let you violate any of Brittle's invariants.  So have fun and remember
:ref:`Rule 6 <rule6>`.

Note that the Object Table itself is an object, and is visible *inside itself*
at slot #1.

.. note:: For more information, see the :ref:`kor-object-table` entry in the
  :ref:`kor`.
