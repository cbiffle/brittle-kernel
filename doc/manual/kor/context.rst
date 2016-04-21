.. _kor-context:

Context
=======

Branding
--------

The brands of Context keys should be zero.


Methods
-------

Read Register (0)
~~~~~~~~~~~~~~~~~

.. warning:: This method is deprecated.

Reads a register, by index, from this context.  Register indices are interpreted
as follows:

- 0 - 15: general-purpose registers ``r0`` - ``r15``.
- 16: program status register.
- 17: ``BASEPRI``.

Call
####

- d0: descriptor
- d1: register index.

Reply
#####

- d0: success descriptor.
- d1: register contents.

Exceptions
##########

- ``k.index_out_of_range`` if the register index is invalid.


Write Register (1)
~~~~~~~~~~~~~~~~~~

.. warning:: This method is deprecated.

Writes a register, by index, in this context.  Register indices are interpreted
as follows:

- 0 - 15: general-purpose registers ``r0`` - ``r15``.
- 16: program status register.
- 17: ``BASEPRI``.

Call
####

- d0: descriptor
- d1: register index
- d2: value

Reply
#####

- d0: success descriptor.

Exceptions
##########

- ``k.index_out_of_range`` if the register index is invalid.


Read Key Register (2)
~~~~~~~~~~~~~~~~~~~~~

Reads a key from one of this context's key registers, by index.  Note that there
are currently 16 key registers.

Call
####

- d0: descriptor.
- d1: key index.

Reply
#####

- d0: success descriptor.
- k1: key from context.

Exceptions
##########

- ``k.index_out_of_range`` if the key index is not valid.


Write Key Register (3)
~~~~~~~~~~~~~~~~~~~~~~

Writes a key into one of this context's key registers, by index.  Note that
there are currently 16 key registers.

Call
####

- d0: descriptor
- d1: key index
- k1: key

Reply
#####

- d0: success descriptor.

Exceptions
##########

- ``k.index_out_of_range`` if the key index is not valid.


Read MPU Region Register (4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reads out the contents of one of this context's MPU region registers.  The
number of MPU region registers per Context is configurable at build time.

Call
####

- d0: descriptor
- d1: region index

Reply
#####

- d0: success descriptor
- k1: region key

Exceptions
##########

- ``k.index_out_of_range`` if the region index is not valid for this Context.


Write MPU Region Register (5)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Alters one of this context's MPU region registers.  The number of MPU region
registers per Context is configurable at build time.

The change takes effect when this context next becomes current, unless it is
already current (i.e. it is modifying itself), in which case it takes effect
immediately, before the reply is sent.

Real memory region keys (to Memory objects) can be loaded directly into the
region registers.  Any other type of key will be treated as a null key and
confer no authority.

.. note:: This is probably going to change; bogus keys should be rejected.

Call
####

- d0: descriptor
- d1: region index
- k1: region key

Reply
#####

- d0: success descriptor

Exceptions
##########

- ``k.index_out_of_range`` if the region register index is not valid for this
  Context.


Make Runnable (6)
~~~~~~~~~~~~~~~~~

Switches this context into "runnable" state.  The practical effect of this
depends on this context's current state:

- If blocked waiting to send or receive, the IPC is interrupted with a
  ``k.would_block`` exception.

- If stopped, the context is simply resumed.

- If already runnable, nothing happens.

.. note::

  Careful reading of this list above will show that a Context trying to make
  *itself* runnable will always succeed but receive an exception.

Call
####

- d0: descriptor

Reply
#####

- d0: success descriptor


Get Priority (7)
~~~~~~~~~~~~~~~~

Gets the current priority of this context.

Call
####

- d0: descriptor

Reply
#####

- d0: success descriptor
- d1: priority

.. warning:: This API may change; priorities may need to be capabilities.


Set Priority (8)
~~~~~~~~~~~~~~~~

Alters the current priority of this context.  If this context is runnable, this
might trigger a context switch.

Call
####

- d0: descriptor
- d1: priority

Reply
#####

- d0: success descriptor

.. warning:: This API may change; priorities may need to be capabilities.



Save Kernel Registers (9)
~~~~~~~~~~~~~~~~~~~~~~~~~

Saves the kernel-maintained registers from this context into memory at
consecutive addresses.  The caller (not the target Context) must have rights to
write those addresses.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster.

The kernel-maintained registers are ``r4`` - ``r11`` and ``BASEPRI``.  When
saved to memory they are written in that order (by ascending address).

Call
####

- d0: descriptor
- d1: destination base address.

Reply
#####

- d0: success descriptor.

Exceptions
##########

- `k.fault` if any of the nine words starting at the destination address cannot
  be written by the caller.

.. warning::

  The way memory authority is conferred in this operation, by implicitly using
  the caller's, is gross and wrong.


Restore Kernel Registers (10)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Restores this context's kernel-maintained registers from consecutive memory
locations.  The caller (not the target Context) must have rights to read from
the memory locations.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster.

The kernel-maintained registers are ``r4`` - ``r11`` and ``BASEPRI``.  When
restored from memory they are read in that order (by ascending address).

Call
####

- d0: descriptor
- d1: source base address.

Reply
#####

- d0: success descriptor.

Exceptions
##########

- `k.fault` if any of the nine words starting at the source address cannot be
  read by the caller.

.. warning::

  If the caller has authority to read only *some* of the memory words, the
  Context's state will be partially restored before the exception is sent.

.. warning::

  The way memory authority is conferred in this operation, by implicitly using
  the caller's, is gross and wrong.
