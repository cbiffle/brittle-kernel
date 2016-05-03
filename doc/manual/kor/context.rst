.. _kor-context:

Context
=======

Branding
--------

The brands of Context keys should be zero.


Methods
-------

.. _kor-context-method-read-register:

Read Register (0)
~~~~~~~~~~~~~~~~~

Reads a register, by index, from this context.  Only the registers specified as
"callee-save" by the ARM Procedure Call Standard are stored in the context;
others are stored on the stack.  Indices for use with this method are as
follows:

===== =======================
Index Register
===== =======================
0     ``r4``
1     ``r5``
2     ``r6``
3     ``r7``
4     ``r8``
5     ``r9``
6     ``r10``
7     ``r11``
8     ``BASEPRI``
9     ``r13`` (stack pointer)
===== =======================

Call
####

- d0: register index.

Reply
#####

- d0: register contents.

Exceptions
##########

- ``k.bad_argument`` if the register index is invalid.


Write Register (1)
~~~~~~~~~~~~~~~~~~

Writes a register, by index, in this context.  Only the registers specified as
"callee-save" by the ARM Procedure Call Standard are stored in the context;
others are stored on the stack.  Indices for use with this method are identical
to those used with :ref:`kor-context-method-read-register`.

Call
####

- d0: register index
- d1: value

Reply
#####

Empty.

Exceptions
##########

- ``k.bad_argument`` if the register index is invalid.


Read Key Register (2)
~~~~~~~~~~~~~~~~~~~~~

Reads a key from one of this context's key registers, by index.  Note that there
are currently 16 key registers.

Call
####

- d0: key index.

Reply
#####

No data.

- k1: key from context.

Exceptions
##########

- ``k.bad_argument`` if the key index is not valid.


Write Key Register (3)
~~~~~~~~~~~~~~~~~~~~~~

Writes a key into one of this context's key registers, by index.  Note that
there are currently 16 key registers.

Call
####

- d0: key index
- k1: key

Reply
#####

Empty.

Exceptions
##########

- ``k.bad_argument`` if the key index is not valid.


Read MPU Region Register (4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reads out the contents of one of this context's MPU region registers.  The
number of MPU region registers per Context is configurable at build time.

Call
####

- d0: region index

Reply
#####

No data.

- k1: region key

Exceptions
##########

- ``k.bad_argument`` if the region index is not valid for this Context.


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

- d0: region index
- k1: region key

Reply
#####

Empty.

Exceptions
##########

- ``k.bad_argument`` if the region register index is not valid for this
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

Empty.

Reply
#####

Empty.


Get Priority (7)
~~~~~~~~~~~~~~~~

Gets the current priority of this context.

Call
####

Empty.

Reply
#####

- d0: priority

.. warning:: This API may change; priorities may need to be capabilities.


Set Priority (8)
~~~~~~~~~~~~~~~~

Alters the current priority of this context.  If this context is runnable, this
might trigger a context switch.

Call
####

- d0: priority

Reply
#####

Empty.

.. warning:: This API may change; priorities may need to be capabilities.


Read (Low/High) Registers (9/10)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reads a block of five kernel-maintained registers from this Context.  There are
ten total such registers, so five are the "low" registers, and five are the
"high".  They are ordered in the same way as for
:ref:`kor-context-method-read-register`.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster.

Call
####

Empty.

Reply
#####

== ====== ======
dn Low    High
== ====== ======
d0 ``r4`` ``r9``
d1 ``r5`` ``r10``
d2 ``r6`` ``r11``
d3 ``r7`` ``r12``
d4 ``r8`` ``r13``
== ====== ======


Write (Low/High) Registers (11/12)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Writes a block of five kernel-maintained registers into this Context.  There are
ten total such registers, so five are the "low" registers, and five are the
"high".  They are ordered in the same way as for
:ref:`kor-context-method-read-register`.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster.

Call
####

== ====== ======
dn Low    High
== ====== ======
d0 ``r4`` ``r9``
d1 ``r5`` ``r10``
d2 ``r6`` ``r11``
d3 ``r7`` ``r12``
d4 ``r8`` ``r13``
== ====== ======

Reply
#####

Empty.
