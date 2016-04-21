Introduction
============

At a high level, Brittle's architecture is...

1. **Object-oriented.**  Every resource and service that the system can see in
   the kernel is represented as an *object* bundling together state and
   operations on that state.

2. **Capability-oriented.**  The only way for a program to refer to objects in
   the kernel --- and thus to inspect them or activate their operations --- is
   by explicit use of a capability, or *key*.  Keys are held by the kernel on
   behalf of programs and cannot be directly inspected or manufactured.

3. **Messaging-oriented.**  The kernel provides programs with a single
   efficient message-transfer operation called *IPC* [#notipc]_ , which is used
   both to operate on kernel objects, and to communicate between application
   tasks.  Messages are small, fixed-size structures that can carry both data
   and keys.

These concepts are closely intertwined.  Programs cannot inspect objects
directly; objects live inside the kernel, and programs are isolated from the
kernel using the ARMv7-M MPU.  If a program wants to read information from the
kernel, or trigger an operation in the kernel, there is only one way to do it:
by sending a *message* to a *key* that refers to the desired *object*.

Resources are managed the same way.  The kernel does not contain an allocator
for memory [#okalloc]_ or other resources.  If a program wants the kernel to
perform an operation that requires resources --- say, create an object that
requires 512 bytes of RAM for bookkeeping --- the program sends a message that
includes a key to the required resources.  This is called a *resource donation*
and is a common pattern in the kernel API.

Because keys are held by the kernel in protected memory, programs cannot, in
general, *escalate* their authority except in carefully designed ways.  A
program can only gain new authority (over kernel facilities *or* resources) by
receiving a key from another program that already had that authority.

.. _null-key:

Certain events can cause an object to be *invalidated* and its keys *revoked*.
All keys to an object are revoked simultaneously, system-wide.  After
revocation, a key behaves as a *null key*, which rejects all IPC operations
with an error code.


.. rubric:: Footnotes

.. [#notipc] This operation is called "IPC" because the term is widely
  understood and roughly accurate, but Brittle's IPC is used for more than
  "inter-process communication."

.. [#okalloc] Okay, technically, the kernel does contain a simple bump-pointer
  allocator used during boot.  It is not, however, used after that, nor is it
  available for use by programs.
