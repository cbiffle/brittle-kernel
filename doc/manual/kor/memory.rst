.. _kor-memory:

Memory
======

A Memory object represents a naturally-aligned power-of-two-sized section of
physical address space.  It may describe RAM, ROM, peripherals, or even
unmapped space that will fault if accessed.

Keys to Memory are what gives programs, running in :ref:`Contexts
<kor-context>`, access to address space.  Without at least one Memory key
loaded in the MPU Region Registers of its Context, a program cannot access any
address space for loads, stores, or even to fetch its own instructions.

Memory objects are also the primary currency used to pay for the creation of
kernel objects (the other being :ref:`slots <kor-slot>`).


Hierarchy
---------

Memory objects can be assembled to form a *hierarchy* by creating new *child*
Memory objects derived from *parent* Memory.  The children have access to a
(non-strict) subset of the parent's address space, and can be separately
revoked.

A *root* Memory is one without a parent.  A set of roots are created during the
:ref:`boot process <boot>`; they can be :ref:`split <memory-method-split>` into
more manageable pieces as needed, but they remain roots.  No two root objects
overlap, so a root and its children have ultimate authority over a section of
address space.

A *leaf* Memory is one without any children.  On startup, all the roots are
also leaves; when a new child is created, it is a leaf; and so on.  Certain
operations that destroy or change the identity of Memory can only be applied to
leaf Memory, to avoid dangling children.

A root which is also a leaf has total authority over a section of address
space: no parents nor children make claims to it.  Such *total* Memory objects
are the only kind that can be donated to the kernel using
:ref:`memory-method-become`.


The Device Attribute
--------------------

Because the ARMv7M architecture memory-maps all peripherals, the same Memory
objects can be used to describe both normal RAM and device I/O space.  This
means it's possible to confuse the two.

In general, the kernel has no opinion on this; see :ref:`Rule 6 (enough rope)
<rule6>`.  However, if applications could trick the kernel into accepting
*donations* of device registers in place of RAM, it could break the kernel's
invariants.

To prevent this, applications can mark some of the root Memory in their AppInfo
struct as *device memory*.  This attribute is sticky: it persists across
splits, and applies to any children.

The kernel will not accept donations of Memory so marked.

.. warning:: There is currently no way for the application to detect device
  memory given a key.  This is an oversight.


Branding
--------

Memory keys use the brand to control application access to the corresponding
section of address space, as well as to determine the memory ordering and cache
behavior of accesses through a particular key.  To reduce possible impedance
mismatches with the hardware, we use the hardware's own encoding: the brand is
*exactly* the value that would be loaded into the MPU's Region Attribute and
Size Register, but right-shifted 8 bits to lop off some irrelevant fields.

.. note:: The intent is to use the top 8 bits to control destroy/split
  permissions in the future; they should be zero for now.

Memory objects will refuse to create keys if the brand specifies an undefined
encoding for the AccessPermission field.


Methods
-------

.. _memory-method-inspect:

Inspect (0)
^^^^^^^^^^^

Returns the actual ARMv7-M MPU settings that would be applied if this Memory key
were loaded into an MPU Region Register of a Context.

.. note:: We use the actual hardware encoding to ensure that the reply is
  honest.  This does make it a little awkward to interpret, though.

Call
####

Empty.

Reply
#####

- d0: Region Base Address Register (RBAR) equivalent contents.
- d1: Region Attribute and Size Register (RASR) equivalent contents.


.. _memory-method-change:

Change (1)
^^^^^^^^^^

Derives a new key to the same Memory object, with the ARMv7-M Region Attribute
and Size Register (RASR) portion of the brand replaced.

This operation is designed to derive weaker keys from stronger ones --- for
example, given a read-write key, derive a read-only one.  As such, the new RASR
value must imply equal or less access than the original, or it will be refused.
This includes the Subregion Disable bits, which can only be set --- not cleared
--- by this operation.

.. note:: It is possible, if slightly weird, to use this operation to create a
  key with *all* Subregion Disable bits set.  Such a key confers no access.

No such validation is applied to the other attribute fields, such as
cacheability and memory ordering.  These fields can be changed arbitrarily.

All bits in the RASR value that are reserved in the ARMv7-M spec should be zero.
The ``SIZE`` and ``ENABLE`` fields in the RASR have no effect on the brand and
should be zero.

Call
####

- d0: new RASR value

Reply
#####

No data.

- k1: new key with requested RASR brand

Exceptions
##########

- ``k.bad_argument`` if the RASR value would increase access, or if it attempts
  to set Subregion Disable bits in a Memory object too small to support them
  (less than 256 bytes in size).


.. _memory-method-split:

Split (2)
^^^^^^^^^

Breaks a Memory object into two equally-sized halves, called bottom and top.
The bottom half starts at the same base address as the original object, but is
half the size; the top half starts just after the bottom half, and is the same
size as the bottom half.  Thus, both halves are still a power of two in size,
and naturally aligned.

This operation produces one net new object.  To justify this use of resources,
callers are required to donate a :ref:`kor-slot` key.  The Slot is consumed and
all keys revoked.

This operation destroys this object, revoking all keys.  Keys to the new Memory
objects representing the top and bottom halves are sent in the reply.  The
returned keys have the same brand as the key used to split.

.. note::
  Splitting is impossible in the following circumstances:

  1. When this Memory object is already the minimum size permitted by the
     architecture (32 bytes).

  2. When the brand of the key used to split has any subregion disable bits set.

  3. When this Memory object has any children.

Call
####

No data.

- k1: slot key being donated

Reply
#####

No data.

- k1: bottom half
- k2: top half

Exceptions
##########

- ``k.bad_operation`` if the region cannot be split for the reasons listed
  above.
- ``k.bad_kind`` if the donated key is not a slot key.


.. _memory-method-become:

Become (3)
^^^^^^^^^^

Uses the address space described by this Memory object to create a new kernel
object of a specified type.

This object must be exactly the same size as the new object (see below).  Sizes
are defined in terms of the configuration-time constant P, the number of
priority levels.

This object must not be device memory.  The kernel only accepts donations of
normal RAM.

If the operation is successful, this object is destroyed, revoking all keys.
The reply message contains the only extant key to the new object, with a default
brand.

.. note:: Currently, "default brand" means zero.  This will be revised.  It
  would be desirable to allow the caller to specify the brand, but currently we
  can't validate the brand until after we destroy the Memory, which would make
  for a bad user experience.

.. note:: Access to this operation will be eventually controlled by the brand.

The type codes for each type of object, the required donation size, and the role
of the message fields/keys are given in the table below.

.. list-table::
  :header-rows: 1

  * - Object Type
    - Type Code
    - Size
    - Data Parameter 2
    - Key Parameter 1
  * - Context
    - 0
    - 512
    - ---
    - Key to unbound Reply Gate
  * - Gate
    - 1
    - 16P
    - ---
    - ---
  * - Reply Gate
    - 2
    - 8 + 8P
    - ---
    - ---
  * - Interrupt
    - 3
    - 32 + 8P
    - Vector number (-1 for SysTick)
    - ---

.. note:: It is not possible to turn a Memory object into another type of
  kernel object if any of the following conditions apply:

  1. If the key used has any subregion disable bits set.

  2. If this Memory object is too big or small for the target object type.

  3. If this Memory object is marked as device memory.

  4. If this Memory object has any children (it is not a leaf).

  5. If this Memory object has a parent (it is not a root).

Call
####

- d0: type code from table above
- d1: type-specific argument

Reply
#####

No data.

- k1: new key

Exceptions
##########

- ``k.bad_argument`` if the object type code is unrecognized.
- ``k.bad_operation`` if this object is not suitable for use with Become, for
  any of the reasons listed above.


.. _memory-method-peek:

Peek (4)
^^^^^^^^

Reads a word of data from the address space corresponding to this Memory object.
For the purposes of this operation, the address space is represented as an array
of words, with the first word at offset zero.

This allows a Memory object to be used without knowing its physical address, and
without having to load it into a Context's MPU Region Register.

The key used must confer read access.

.. warning:: Currently, the operation is performed without regard for the
  ordering and cache behaviors specified by the key.  This is not deliberate.

Call
####

- d0: offset

Reply
#####

- d0: word of data

Exceptions
##########

- ``k.bad_argument`` if the offset is out of range.
- ``k.bad_operation`` if the key used does not confer read access.

.. _memory-method-poke:

Poke (5)
^^^^^^^^

Writes a word of data into the address space corresponding to this Memory
object.  For the purposes of this operation, the address space is represented as
an array of words, with the first word at offset zero.

This allows a Memory object to be used without knowing its physical address, and
without having to load it into a Context's MPU Region Register.

The key used must confer write access.

.. warning:: Currently, the operation is performed without regard for the
  ordering and cache behaviors specified by the key.  This is not deliberate.

Call
####

- d0: offset
- d1: word of data

Reply
#####

No data.

Exceptions
##########

- ``k.bad_argument`` if the offset is out of range.
- ``k.bad_operation`` if the key used does not confer write access.


Make Child (6)
^^^^^^^^^^^^^^

Makes a new child Memory object, with this object as its parent.

The child can describe any (appropriately aligned) subset of this object's
address space.  It will inherit any device attribute, and the initial child key
inherits the access permissions from the key used to invoke this method.

As this creates a net new object, a :ref:`slot key <kor-slot>` donation is
required.

Call
####

- d0: base address of child.
- d1: size of child, given as log2(size) - 1.
- k1: slot key to donate.

Reply
#####

No data.

- k1: key to child object.

Exceptions
##########

- ``k.bad_operation`` if the key used to invoke this object has subregion
  disable bits set.
- ``k.bad_argument`` if the given base/size is outside the parent's address
  space.
- ``k.bad_kind`` if the alleged slot key is not, in fact, a slot key.
