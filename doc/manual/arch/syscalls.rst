Syscalls
========

Brittle implements a small set of *syscalls*, which act as virtual instructions
that extend the processor.  Syscalls exist for two reasons:

1. To let programs manipulate the Key Registers that Brittle adds to the
   processor model.

2. To let programs communicate with one another.

Syscalls are accessed using the ARMv7-M ``SVC`` instruction.

.. note:: Brittle currently hooks every ``SVC``, which means it can't virtualize
  other operating systems' syscalls.  We are likely to fix this by adding a
  "foreign" bit to the :ref:`kor-context`.


Syscall Descriptor Convention
-----------------------------

Every Brittle syscall requires a descriptor loaded in processor register ``r4``.
The top four bits of the descriptor are a *sysnum*, or syscall number, which
determines the operation to perform.

====== ============
Sysnum Operation
====== ============
0      IPC
1      Copy Key
2      Discard Keys
====== ============

The remaining 28 bits of the descriptor are interpreted differently by each
syscall.


Copy Key
--------

Reads a key from one of the current Context's Key Registers, and writes a
duplicate of it into another.  All processor registers are left unchanged.

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
    - 1 (Copy Key)
    - (preserved)
  * - 27
    - 24
    - Source
    - Index of Key Register containing key to copy.
    - (preserved)
  * - 23
    - 20
    - Target
    - Index of Key Register to receive copy.
    - (preserved)
  * - 19
    - 0
    - Reserved
    - Should be zero.
    - (preserved)


Discard Keys
------------

Writes :ref:`kor-null` keys into some of the current Context's Key Registers.
This can be used to discard authority, or prepare for sending a message without
keys.

Any contiguous range of registers can be specified.  Registers are given by the
register indices of the first and last affected register, inclusive.  If the
last index is smaller than the first, no registers are affected.

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
    - 2 (Discard Keys)
    - (preserved)
  * - 27
    - 24
    - First
    - Index of first Key Register to discard.
    - (preserved)
  * - 23
    - 20
    - Last
    - Index of last Key Register to discard.
    - (preserved)
  * - 19
    - 0
    - Reserved
    - Should be zero.
    - (preserved)


IPC
---

Brittle is built around a synchronous rendezvous messaging model.  This means
that messages are sent from one object to another directly, without being
buffered in the kernel.

Programs interact with objects using the ``IPC`` system call, which defines an
operation consisting of two optional *phases*:

- The *send phase* transmits a message from the sender's Context to an object
  using a key held in a key register.

- The *receive phase* receives a message from a remote object, again using a key
  held in a key register.

.. note:: Microkernel fans will recognize this as being very similar to
  Liedtke's ``SendAndWait`` mechanism from L4.

The two phases can be combined into three types of IPC operations, plus a fourth
special variety:

1. *Send.*  A message is sent; once delivery of the message is accepted, the
   sender resumes without waiting for a response.  This operation can optionally
   be marked *non-blocking*; a non-blocking send does not wait for delivery to
   be accepted if the recipient is not ready.

2. *Receive.*  The program retrieves a message through a key (which should be a
   Gate key).  If a message is already available, this operation completes
   immediately.  If no message is available, the program blocks.  (Receive
   operations are always potentially blocking.)

3. *Send-then-receive.*  This performs a send, atomically followed by a receive,
   potentially on two unrelated keys.  It is typically used by server programs
   to reply to one request and accept the next.

4. *Call.*  A specialized form of send-then-receive, a call IPC fabricates a
   *reply key* for the program's bound Reply Gate and sends it with the message.
   The object receiving the message can use the reply key to issue a single
   reply back to the sender.

.. note:: Technically there is a fifth variety: if neither phase is requested,
  the syscall simply returns to the caller.  This is not very interesting.

Each phase of an IPC transfers a single message.  A message consists of

- Four words of data, one of which is a descriptor that controls the messaging
  operation.

- Three keys.

When a program receives a message, one more word of data is included: the brand
of the key used to send the message through a Gate.

Message Descriptors
~~~~~~~~~~~~~~~~~~~

The first word in a message is called the *descriptor*, and controls the IPC operation.  Its fields are as follows.

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


The Send Phase
~~~~~~~~~~~~~~

A sent message contains

- Four data words taken from registers ``r4`` through ``r7``, where ``r4`` is
  the descriptor.
- Four keys taken from key registers ``k0`` through ``k3``.

If the IPC operation is a call, the first key transmitted is not taken from
``k0``, but is rather a freshly-minted Reply Key.


The Receive Phase
~~~~~~~~~~~~~~~~~

A received message contains

- Four data words in registers ``r4`` through ``r7``, where ``r4`` is the
  descriptor.
- Four keys, in key registers ``k0`` through ``k3``.
- The brand of the Gate key used to send the message, in ``r8``.

The received descriptor is *sanitized*: the key index fields are zeroed, so that
the recipient doesn't learn anything about how the sender organizes their keys.

The key delivered in ``k0`` is conventionally a reply key, whether it's a
real-live key to a Reply Gate, or not.  Servers that expect call-style IPCs
agree to send a response back on the reply key in ``k0``.

The received Gate key brand can be used to distinguish callers from one another,
encode application-defined permissions, etc.
