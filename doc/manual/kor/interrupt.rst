.. _kor-interrupt:

Interrupt
=========

An "interrupt" object represents a hardware interrupt, manages its enabled
status, and generates messages when the interrupt occurs.

The messages go to a "target" key, loaded by the Set Target method.  The target
may be null.

The interrupt is initially disabled, so no messages will be sent until the
object receives an Enable message.  (No messages will be sent anywhere *useful*
until it also receives a Set Target message.)

The interrupt is disabled when the message is generated, and remains disabled
until the object receives another Enable message.  The Enable message has the
option of clearing any potentially queued interrupts that arrived while the
interrupt was disabled, or leaving them enabled so they will be processed
immediately.


Branding
--------

Interrupt key brands should be zero.


.. _interrupt-methods:

Methods
-------

Set Target (1)
~~~~~~~~~~~~~~

Loads a new target key for this interrupt.  When the interrupt fires it will be
converted into a message to the target key.

Any existing target key will be discarded.

The target key can be null, to disable delivery.

Call
####

No data.

- k1: target

Reply
#####

Empty.


Enable (2)
~~~~~~~~~~

Enables the interrupt, acknowledging any outstanding interrupt.

Call
####

- d0: clear pending (1) or leave as-is (0).

Reply
#####

Empty.
