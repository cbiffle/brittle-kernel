.. _boot-process:

The Boot Process
================

Introduction
------------

The Brittle kernel is designed to be linked into a ROM image, together with the
system/application.  The ROM image includes a standard ARMv7-M vector table,
which gives the hardware (or a bootloader) the initial stack pointer and program
entry point for the kernel.

The kernel initializes its memory, configures required hardware (particularly
the Memory Protection Unit), and then starts the application initialization
process.  This process consists of three phases:

1. Creating certain well-known kernel objects that are required for correct
   operation.

2. Processing the App Info Block to create any additional kernel objects
   required by the application.

3. Configuring the First Context to run the application's entry point, and
   starting it.


The Well-Known Objects
----------------------

There are four "well-known" objects that exist in every Brittle application.
They are assigned fixed indices in the Object Table.

.. list-table::
  :header-rows: 1

  * - Index
    - Type
    - Role
  * - 0
    - :ref:`kor-null`
    - The null object.
  * - 1
    - :ref:`kor-object-table`
    - The object table itself.
  * - 2
    - :ref:`kor-context`
    - The First Context, which is started at the end of initialization.
  * - 3
    - :ref:`kor-reply-gate`
    - The Reply Gate for the First Context.


The App Info Block
------------------

The App Info Block instructs the kernel on how to start the application.  It
gets linked into the ROM image immediately after the end of the kernel.  The
precise definition and structure are in ``common/app_info.h`` in the sources, so
the discussion below may be out of date.





The First Context's Environment
-------------------------------


