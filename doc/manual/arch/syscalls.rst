.. _syscalls:

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


IPC
---

Brittle is built around a synchronous rendezvous messaging model.  This means
that messages are sent from one object to another directly, without being
buffered in the kernel.

Programs interact with objects using the ``IPC`` system call, which defines an
operation consisting of two optional *phases*:

- The *send phase* transmits a message from the sender's Context to an object
  designated by a key held in a key register.

- The *receive phase* receives a message from a remote object, again designated
  by a key held in a key register.

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
   *reply key* for the program's Context and sends it with the message.  The
   object receiving the message can use the reply key to issue a single reply
   back to the sender.

.. note:: Technically there is a fifth variety: if neither phase is requested,
  the syscall simply returns to the caller.  This is not very interesting.

Each phase of an IPC transfers a single message.  A message consists of

- The syscall descriptor.

- Five words of data.

- Four keys.

When a program receives a message, one more datum is included: the brand of the
key used to send the message through a Gate.


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
    - Error in operation.
  * - 15
    - 0
    - Selector
    - Varies
    - Varies


Key Maps
~~~~~~~~

When keys are sent from or received into key registers, the registers are chosen
according to an additional syscall parameter, the *key map*.  A key map packs
several four-bit key register indices into a single word.

For the purposes of this section, and for the definition of kernel methods
presented in the :ref:`kor`, the registers named by the four positions in the
keymap will be referred to as ``k0`` through ``k3``.

.. list-table:: Key Map Bit Fields
  :header-rows: 1

  * - Hi
    - Lo
    - Name
  * - 15
    - 12
    - k3
  * - 11
    - 8
    - k2
  * - 7
    - 4
    - k1
  * - 3
    - 0
    - k0

.. note:: The top 16 bits of the key map are currently unused, to allow for
  future expansion.  These bits should be zero.

The same register index may appear *multiple times* in a key map.  For sent
keys, this causes the same key to be sent in multiple positions.  For received
keys, this causes multiple keys to be delivered to the same register, and
*it is not defined* which comes last.

A Context's key register 0 permanently contains a key to :ref:`kor-null`.  This
means register index 0 can be used in a key map in any "don't-care" positions
without accidentally transmitting or receiving authority.


The Send Phase
~~~~~~~~~~~~~~

A sent message contains

- The descriptor in ``r4``.
- Five data words taken from registers ``r5`` through ``r9``.
- A key map in ``r10``.
- Four keys taken from the ``k0`` - ``k3`` registers named by the key map.

If the IPC operation is a call, the first key transmitted (``k0``) is not chosen
by the key map, but is rather a freshly-minted Reply Key.

A blocking send phase indicates success by continuing to the next phase (if
any).  A non-blocking send phase cannot indicate success or failure.


The Receive Phase
~~~~~~~~~~~~~~~~~

The receive phase uses the same descriptor in ``r4`` as the send phase, but
ignores the contents of ``r5`` through ``r10``.  IPC involving a receive phase
takes an additional key map in ``r11``.

After receipt of a message, the program gets:

- A sanitized version of the descriptor in ``r4``.
- Five data words in registers ``r5`` through ``r9``.
- The brand of the Gate key used to send the message, in ``r10``/``r11``.
- Four keys, delivered into the ``k0`` - ``k3`` registers named by the key map.

The received descriptor is *sanitized*: the key index fields are zeroed, so that
the recipient doesn't learn anything about how the sender organizes their keys.

The key delivered into the first position chosen by the key map (``k0``) is
conventionally a reply key, whether it's a real-live Reply Key to a Context, or
something else (such as a Gate key for a testing framework).  Servers that
expect call-style IPCs agree to send a response back on the reply key.

The received Gate key brand (in ``r10``/``r11``) can be used to distinguish
callers from one another, encode application-defined permissions, etc.

If a program tries to receive using a key that doesn't permit it (including
keys to objects that are not Gates) it will instead receive an exception
message.
