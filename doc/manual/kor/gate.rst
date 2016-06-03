.. _kor-gate:

Gate
====

Gates provide a rendezvous point for Contexts (and optionally Interrupts) using
IPC.


Branding
--------

Gate key brands separate Gate keys into two groups: *service keys* and
*transparent keys* (or client keys).  The two groups are distinguished by the
MSB of the brand (set for transparent, clear for service).

Only service keys have access to the methods described below.  Any invocations
on a transparent key flow right through the Gate to any Context receiving on
the far side (using a service key).

.. _kor-gate-service-key:

Service Keys
~~~~~~~~~~~~

Service keys have the top bit *clear*.  Programs holding a service key can
invoke the methods described below.

Remaining bits in the service key brand are reserved and should be zero.  That
is, in the version described by this manual, the only valid service key brand
is ``Brand(1) << 63``.

Programs can *receive* from a Gate service key, which will rendezvous with
another program sending through a transparent key, blocking if necessary.
Because the sender is always using a transparent key, any message received in
this way will have the top bit of the sender brand *set*.

.. _kor-gate-transparent-key:

Transparent Keys
~~~~~~~~~~~~~~~~

Transparent keys have the top bit *set*.  The brand is passed, otherwise
uninterpreted, to a program using a *receive* IPC on a service key.

.. _gate-methods:

Methods
-------

.. _gate-method-make-client-key:

Make Client Key (1)
~~~~~~~~~~~~~~~~~~~

Derives a transparent (client) key for this gate with the given brand.  The
brand must be a valid brand for a transparent key --- that is, it must have its
MSB set.

This operation is intended for servers that want to distribute identifiable
keys to different clients.

Call
####

- d0: brand low 32 bits.
- d1: brand high 32 bits.

Reply
#####

- k1: derived key.

Exceptions
##########

- ``k.bad_argument`` if the given brand does not have its MSB set.
