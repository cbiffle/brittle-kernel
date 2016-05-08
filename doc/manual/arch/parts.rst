Kernel, System, Application
===========================

Viewed simply, any piece of code is either part of the kernel, or not.  This
distinction is often cast (thanks to Unix) as that of kernel vs. userland.  In
a Unix-style monolithic kernel like Linux, system calls are provided by the
kernel, and are fundamentally different from calls implemented by libraries or
inter-process communication.

The situation is different with a microkernel like Brittle.  The kernel is
designed to be extended by programs running in "userland," and the equivalent
of "system calls" --- messages to objects --- behaves the same whether it acts
on kernel-implemented objects or components of an application.  This means the
object model presented by the kernel is *extensible*: programs can define new
object types, proxy existing ones to apply access control policies, or even
emulate a newer or older kernel API version, and other programs *can't tell the
difference*.

This gives Brittle an excuse for its rather aggressive minimality, and
insistence on implementing mechanism over policy.  By not including (for
example) a notion of IPC timeouts in the kernel, Brittle is not saying your
application *shouldn't* use IPC timeouts!  Rather, the kernel doesn't *need* IPC
timeouts to function, itself, and moreover isn't sure how exactly you'd like
timeouts to behave.  Rather than providing a particular scheme that you'd have
to live with or work around, Brittle leaves the decision up to you.  In general,
Brittle does this with everything it thinks it can get away with; for more
details, see the section :ref:`kernel-scope`.

Does this mean that every application should implement its own SysTick driver,
time-slicing, and the like?  *Certainly not.*  We expect this sort of thing will
get wrapped up into a reusable layer between the kernel and applications.  We
refer to this layer as *the System*.  It is *unprivileged but trusted* code.

The System is *unprivileged* because, like all programs on the Brittle kernel,
it runs in the processor's unprivileged mode.  This means there are things it
cannot do, mostly related to subverting the kernel's invariants.

At the same time, the System is *trusted* because it has tremendous authority
over the functioning of the application.  Indeed, when Brittle boots the first
program (assumed to be part of the System), it hands that program a key that
grants explicit authority to subvert all compartmentalization and isolation
outside the kernel!  This authority is scary but vital; the System may have a
legitimate need to peer into isolated programs' memory, for example to
implement Unix-like ``read`` and ``write`` operations.  A trusted System will
use this power for good, in support of the application's mission, and not (for
example) redistribute it to potentially malicious programs downloaded from the
network.  (See the section on the :ref:`boot process <boot>` to learn more
about the roots of authority.)

.. note:: The distinction between kernel and System will be familiar to
  adherents of L4, but may seem more foreign to people familiar with the
  KeyKOS/EROS family of systems.  (They have a similar distinction but it's
  less clear in the documentation.)

  Among first-generation microkernels, the role of the System is most similar
  to the idea of "kernel personalities" from Mach.
