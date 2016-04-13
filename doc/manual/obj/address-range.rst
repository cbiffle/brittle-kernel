.. _kor-address-range:

Address Range
=============

An Address Range object represents an arbitrary span of physical memory, with
limited definition of access rights (e.g. read-only, no-execute).

Keys to the Address Range contain substantially more information than the
object itself does, and are used to model all access to memory and peripherals;
see :ref:`address-range-branding` below.


Initialization Properties
-------------------------

- Base address
- End address
- Prevent code execution (1) or permit (0)
- Read-only (1) or read-write (0)

.. _address-range-branding:

Branding
--------

Address Range keys act as (and are often referred to) "memory regions."  The 64
bits of the brand are used to store the equivalent MPU memory region settings,
in the following format:

- Region Base Address Register (RBAR) in low 32 bits.
- Region Attribute and Size Register (RASR) in high 32 bits.

The following validity rules are enforced:

1. All bits reserved in the ARMv7-M PMSA standard must be zero.

2. The ``REGION`` and ``VALID`` fields of the RBAR must be zero.

3. The size described must be at least 32 bytes.

4. The entire extent of the described region must fall within this Address
   Range.

5. The Access Permissions in the RASR must be a legal value per the ARMv7-M
   PMSA standard, *and* must allow at least read access to privileged code.

6. The Access Permissions must not allow any access prevented by this Address
   Range's initialization parameters (e.g. if this Address Range is set to
   disallow code execution, so must all its region keys be).


Methods
-------

Inspect Region (0)
^^^^^^^^^^^^^^^^^^

Returns the actual ARMv7-M MPU settings that would be applied if this region
were loaded into a Region Register of a Context.

Call
####

- d0: descriptor

Reply
#####

- d0: success descriptor
- d1: Region Base Address Register (RBAR) equivalent contents.
- d2: Region Attribute and Size Register (RASR) equivalent contents.

.. note:: The encoding of the reply is awkward, but honest.


Split Region (1)
^^^^^^^^^^^^^^^^

Derives two new region keys, representing the bottom and top halves of this
region.  Other attributes (read/write/execute, cacheability, etc.) are
unchanged.

Any areas of this region rendered inaccessible by subregion disable bits will
remain inaccessible in each of the derived regions, by expanding each disabled
region to affect two adjacent (and half-sized) subregions.

The original key is not altered or revoked.

.. note::

  Splitting is not possible when:

  1. The memory region is already the minimum size recognized by the
     architecture (32 bytes).

  2. The memory region has disabled subregions, and is already the minimum size
     permitted for use with subregions (256 bytes).

Call
####

- d0: descriptor

Reply
#####

- d0: success descriptor
- k1: bottom half
- k2: top half

Exceptions
##########

- ``k.region_too_small`` if the region cannot be split.


Disable Subregions (2)
^^^^^^^^^^^^^^^^^^^^^^

Derives a new region key with some subregions (possibly all!) disabled.  The
given disable mask is bitwise-ORed with the original disable mask, so any
subregions already disabled stay that way.

The original key is not altered or revoked.

.. note::

  ARMv7-M does not allow subregions to be disabled in regions smaller than 256
  bytes, so neither do we.

Call
####

- d0: descriptor
- d1: disable mask (in low 8 bits)

Reply
#####

- d0: success descriptor
- k1: new key

Exceptions
##########

- ``k.region_too_small`` if the region is too small for the use of subregion
  disable bits.


Drop Privileges (3)
^^^^^^^^^^^^^^^^^^^

Derives a new region key with certain privileges missing, depending on the
arguments.  (If this key didn't *have* the privileges in question, then the new
key is identical.)

The original key is not altered or revoked.

Call
####

- d0: descriptor
- d1: privileges to drop (bitmask)
  - bit 0: execute
  - bit 1: write
  - bit 2: read

Reply
#####

- d0: success descriptor
- k1: new key
