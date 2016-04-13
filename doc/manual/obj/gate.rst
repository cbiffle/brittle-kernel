.. _kor-gate:

Gate
====

Gates provide a rendezvous point for Contexts (and optionally Interrupts) using
IPC.

Gates have no native protocol or methods.  Any message sent to a Gate is
transferred to a Context receiving from the Gate, blocking if necessary.


Initialization Properties
-------------------------

None.


Branding
--------

Gate key brands are passed, uninterpreted, to Contexts that receive from the
Gate.

The top 32 bits of Gate brands should be zero for future expansion.

The bottom 32 bits can be used for arbitrary application purposes.


Methods
-------

Gates have no inherent methods.
