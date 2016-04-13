Introduction
============

Status
------

Brittle is immature.  It has been used in some demos.  Parts of the design are
still in flux.


What Brittle Is
---------------

Brittle is a microkernel intended for high-reliability embedded applications on
ARMv7-M architecture processors, such as the Cortex-M4.  These processors don't
have a conventional Memory Management Unit, which limits their ability to run
traditional operating systems with (paged) memory protection.  Instead, Brittle
is designed to use the ARMv7-M Memory Protection Unit to provide isolation.

Brittle is a *third-generation* microkernel.  Its design is heavily inspired by
EROS/KeyKOS, MINIX 3, and the L4 family, particularly seL4.

Like other third-generation microkernels (broadly speaking), Brittle...

* Focuses on minimality and security,

* Expresses all authority through explicit *capabilities*,

* Moves other mechanisms with security implications outside the kernel,

* Blurs the line between a traditional microkernel and a hypervisor, and

* Targets a very small kernel codebase (in Brittle's case, less than 2500
  ``sloccount`` lines of code).

*Unlike* its peers, Brittle explicitly targets systems with between 16 and 200
*kiB* of RAM.


What Brittle Isn't
------------------

Brittle is not a complete operating system; it is only a kernel.

By analogy: putting the Linux kernel on a computer doesn't allow one to run
Firefox.  Linux is only a kernel; there's a whole lot of operating system that
must be added to make a complete system.

This is even more true of Brittle than of Linux, because Brittle's design is so
minimal.  Like other third-generation microkernels, Brittle doesn't even
include hardware drivers in the kernel.

You can write an application around the kernel directly, but the intent is that
a *system layer* wraps the kernel and provides common reusable services that
the kernel does not, such as mutexes and timers, or even a full POSIX model.
Applications would then be written to target this system layer.

The system layer also insulates applications from the details of the hardware,
because the Brittle kernel itself is *explicitly not portable*.  Its design is
ARMv7-M-specific, and exposes ARMv7-M-specific APIs and abstractions.
Applications that wish to run on other types of processors should be written to
a portable abstraction layer, provided outside the kernel by the system.  (I
don't consider Brittle's non-portability to be a problem, because the entire
kernel sources are significantly smaller than the architecture-specific support
code for a single CPU in most kernels.)
