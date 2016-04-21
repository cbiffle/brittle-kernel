.. _kor-gate:

Gate
====

Gates provide a rendezvous point for Contexts (and optionally Interrupts) using
IPC.

Gates have no native protocol or methods.  Any message sent to a Gate is
transferred to a Context receiving from the Gate, blocking if necessary.


Branding
--------

Gate key brands are passed, uninterpreted, to Contexts that receive from the
Gate.

The kernel reserves the top 8 bits.


Methods
-------

Gates have no inherent methods.
