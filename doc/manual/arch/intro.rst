Basic Concepts
==============

This section introduces some ideas that will help you follow the next few
sections, which elaborate the same ideas in more detail.

At a high level, Brittle's architecture is...

1. **Object-oriented.**  Every resource and service that the system can see in
   the kernel is represented as an *object* bundling together state and
   operations on that state.

2. **Capability-oriented.**  The only way for a program to refer to objects in
   the kernel --- and thus to inspect them or activate their operations --- is
   by explicit use of a capability, or *key*.  A key is a combination of an
   object reference and a set of *rights*, operations on the object that a
   program holding the key can perform.  Keys are held by the kernel on behalf
   of programs and cannot be directly inspected or manufactured.

3. **Messaging-oriented.**  The kernel provides programs with a single
   efficient message-transfer operation called *IPC* [#notipc]_ , which is used
   both to operate on kernel objects, and to communicate between application
   tasks.  Messages are small, fixed-size structures that can carry both data
   and keys.

These concepts are closely intertwined.  Programs cannot inspect objects
directly; objects live inside the kernel, and programs are isolated from the
kernel using the ARMv7-M MPU.  If a program wants to read information from the
kernel, or trigger an operation in the kernel, there is only one way to do it:
by sending a *message* to a *key* that refers to the desired *object*, and
receiving another message in response.

Because keys are held by the kernel in protected memory, programs cannot, in
general, *escalate* their authority except in carefully designed ways.  A
program can only gain new authority (over kernel facilities *or* resources) by
receiving a key from another program that already had that authority.

Resources are managed the same way.  The kernel does not contain an allocator
for memory [#okalloc]_ or other resources.  If a program wants the kernel to
perform an operation that requires resources, it must *already hold* the
resources, and must *donate* them to the kernel in exchange for a key to the
desired new object.  Resources are named by keys and donated in messages.  For
example, to create a new kernel object that requires 512 bytes of RAM for
book-keeping, a program would send a message including a key to a 512-byte
:ref:`kor-memory` object.

Certain events can cause an object to be *invalidated* and its keys *revoked*.
This causes all keys to that object to simultaneously and atomically become
*null keys* (keys to an object called :ref:`kor-null`), which reject all IPC
operations with an error code.  For example, when a program donates resources
(i.e. an object) to the kernel, the kernel invalidates the donated object,
which ensures that no program continues to hold keys to it.  Thus control of
the object is reliably taken away from the program.


.. rubric:: Footnotes

.. [#notipc] This operation is called "IPC" because the term is widely
  understood and roughly accurate, but Brittle's IPC is used for more than
  "inter-process communication."

.. [#okalloc] Okay, technically, the kernel does contain a simple bump-pointer
  allocator used during boot.  It is not, however, used after that, nor is it
  available for use by programs.
