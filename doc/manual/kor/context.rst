.. _kor-context:

Context
=======

A *Context* models the processor's unprivileged state with Brittle-specific
extensions:

- The sixteen general-purpose registers plus the program status word, for
  storing the program's execution state.
- A separate copy of the ``BASEPRI`` register, so that separate Contexts can
  mask and unmask interrupts independently.
- Sixteen *Key Registers* for storing keys.
- A configurable number [#configmpu]_ of *MPU Region Registers* for defining
  the program's address space.

One Context is created by the kernel during :ref:`boot <boot>`.

Programs can create additional Contexts using the :ref:`memory-method-become`
method on :ref:`kor-memory`.

Branding
--------

Brands separate Context keys into two classes, *reply keys* and *service keys*.

Reply Keys
~~~~~~~~~~

A reply key grants the authority to deliver a single reply message to the
program inhabiting the Context.  Reply keys have the top bit of their brand
*set*; the other 63 bits encode a *call count*.  The Context internally tracks
the call count, and only honors reply keys with the right count, to ensure that
reply keys cannot be reused.  (The call count is incremented at each successful
reply, and whenever :ref:`context-method-make-runnable` is used to interrupt a
Context awaiting reply.)

Reply keys are *transparent* --- they implement no native methods, and instead
just forward any received messages through to the program inhabiting the
Context.

The only way to obtain a reply key [#guessing]_ is by invoking a key with a
call-style invocation.  The kernel will internally fabricate a reply key and
send it with the message.  When the send phase completes, the Context
atomically switches to expecting a reply through a reply key.

Service Keys
~~~~~~~~~~~~

A service key grants the authority to perform maintenance on the Context, e.g.
inspecting and altering register values.  Service keys have the top bit of
their brand *clear*, and implement the methods described below.

.. warning:: Currently, any Context service key can be used to call any method
  listed below.  This is temporary.  The service key brand will be further
  defined to allow weakened service keys.


Methods
-------

.. _context-method-read-register:

Read Register (0)
~~~~~~~~~~~~~~~~~

Reads a register, by index, from this Context.  Only the registers specified as
"callee-save" by the ARM Procedure Call Standard, plus ``BASEPRI`` and the
stack pointer,  are stored in the Context; others are stored on the stack.
Indices for use with this method are as follows:

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

.. warning:: Because this method reads a *single* register per IPC, it can
  become a bottleneck for programs (like debuggers) that need to read the
  entire register set.  For better performance in such programs, see
  :ref:`context-method-read-registers`.

Call
####

- d0: register index.

Reply
#####

- d0: register contents.

Exceptions
##########

- ``k.bad_argument`` if the register index is invalid.


.. _context-method-write-register:

Write Register (1)
~~~~~~~~~~~~~~~~~~

Writes a register, by index, in this context.  Only the registers specified as
"callee-save" by the ARM Procedure Call Standard, plus ``BASEPRI`` and the
stack pointer,  are stored in the context; others are stored on the stack.
Indices for use with this method are identical to those used with
:ref:`context-method-read-register`.

.. warning:: Because this method writes a *single* register per IPC, it can
  become a bottleneck for programs (like debuggers) that need to write the
  entire register set.  For better performance in such programs, see
  :ref:`context-method-write-registers`.

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


.. _context-method-read-key-register:

Read Key Register (2)
~~~~~~~~~~~~~~~~~~~~~

Reads a key from one of this Context's key registers, by index.  Note that there
are currently 16 key registers.

Key register 0 always reads as null.

Call
####

- d0: key index.

Reply
#####

No data.

- k1: key from Context.

Exceptions
##########

- ``k.bad_argument`` if the key index is not valid.


.. _context-method-write-key-register:

Write Key Register (3)
~~~~~~~~~~~~~~~~~~~~~~

Writes a key into one of this Context's key registers, by index.  Note that
there are currently 16 key registers.  Because key register 0 is permanently
null, index 0 is treated as invalid for this operation and will return an
exception.


Call
####

- d0: key index
- k1: key

Reply
#####

Empty.

Exceptions
##########

- ``k.bad_argument`` if the key index is not valid (including 0).


.. _context-method-read-mpu-region-register:

Read MPU Region Register (4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reads out the contents of one of this Context's MPU region registers.  The
number of MPU region registers per Context is configurable at build time
[#configmpu]_.

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


.. _context-method-write-mpu-region-register:

Write MPU Region Register (5)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Alters one of this Context's MPU region registers.  The number of MPU region
registers per Context is configurable at build time [#configmpu]_.

The change takes effect when this Context next becomes current, unless it is
already current (i.e. it is modifying itself), in which case it takes effect
immediately, before the reply is sent.  This implies that a Context using this
operation to remove its own access to stack will never receive the reply, taking
a fault instead.

Real :ref:`kor-memory` keys can be loaded directly into the region registers.
Any other type of key will be treated as a null key and confer no authority.

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


.. _context-method-make-runnable:

Make Runnable (6)
~~~~~~~~~~~~~~~~~

Switches this Context into "runnable" state.  The practical effect of this
depends on this Context's current state:

- If blocked waiting to send, without receive phase, the send is cancelled and
  the Context receives no notification.

- If blocked in receive, without send phase, the Context immediately receives
  a ``k.would_block`` exception.  

- If blocked in send-receive or call, the send phase is skipped and the Context
  immediately receives a ``k.would_block`` exception.

- If stopped, the Context is simply resumed.

- If already runnable, nothing happens.

.. note::

  Careful reading of this list above will show that a Context trying to make
  *itself* runnable will always succeed but receive an exception anyway.

Call
####

Empty.

Reply
#####

Empty.


.. _context-method-get-priority:

Get Priority (7)
~~~~~~~~~~~~~~~~

Gets the current priority of this Context.

Call
####

Empty.

Reply
#####

- d0: priority

.. warning:: This API may change; priorities may need to be capabilities.


.. _context-method-set-priority:

Set Priority (8)
~~~~~~~~~~~~~~~~

Alters the current priority of this Context.  If this Context is runnable, this
might trigger a Context switch.

Call
####

- d0: priority

Reply
#####

Empty.

.. warning:: This API may change; priorities may need to be capabilities.


.. _context-method-read-registers:

Read (Low/High) Registers (9/10)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each of these two methods reads a block of five kernel-maintained registers
from this Context.  There are ten total such registers, so five are the "low"
registers, and five are the "high".  They are ordered in the same way as for
:ref:`context-method-read-register`.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster, and is also useful in debuggers.

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


.. _context-method-write-registers:

Write (Low/High) Registers (11/12)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each of these two methods writes a block of five kernel-maintained registers
into this Context.  There are ten total such registers, so five are the "low"
registers, and five are the "high".  They are ordered in the same way as for
:ref:`context-method-read-register`.

This operation is intended to make "swapping" --- multiplexing multiple logical
tasks across a single Context --- faster, and is also useful in debuggers.

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


.. rubric:: Footnotes

.. [#configmpu] The number of MPU region registers can be configured at build
  time.  The current default is six.  The number of MPU region registers must be
  no larger than the actual number of MPU regions implemented by the hardware.

.. [#guessing] Except by guessing, of course.  With an :ref:`kor-object-table`
  key a program can fabricate fake reply keys.  If it can guess the call count,
  it can fake a reply.  This is one reason why call counts are so large (63
  bits).  Currently they're zeroed at Context creation, but we may randomize
  them in the future to make guessing *really hard*.
