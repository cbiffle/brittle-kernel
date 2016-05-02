.. _kor-memory:

Memory
======

A Memory object represents a naturally-aligned power-of-two-sized section of
physical address space.  It does not specify whether that address space
contains RAM, ROM, devices, or anything at all.

Keys to a Memory object use the brand to refine this, specifying all the
attributes supported by the ARMv7-M MPU, including read and execute
permissions, access ordering, and cache behavior.

A certain set of Memory objects are created during the :ref:`boot process
<boot>`.  These objects are expected to be somewhat large, and can be
:ref:`split <memory-method-split>` into more manageable sizes.

If a Memory object has the same size as a kernel object of another type, it can
be donated to the kernel to create a new object using the :ref:`become
<memory-method-become>` operation.  This destroys the donated memory object,
revoking all keys.

Because Memory objects are *destroyed* to create kernel objects, the section of
address space controlled by a Memory object is *inherently* under application
(not kernel) control.  Applications can access that address space in two ways:

1. A key to a Memory object may be loaded into the MPU Region Registers of a
   :ref:`Context <context-object>`, causing the corresponding section of address
   space to become directly accessible to the program inhabiting that Context.  

2. Programs can send the :ref:`peek <memory-method-peek>` and :ref:`poke
   <memory-method-poke>` messages to a Memory key to treat the address space
   like an array or storage device.



.. note:: Eventually, the intent is to have a "destroy" verb on other objects,
  to reclaim their Memory, and a "merge" operation on Memory to reverse splits.


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
- ``k.bad_operation`` if the Memory key used has subregion disable bits set, or
  is the wrong size for the requested object.


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
