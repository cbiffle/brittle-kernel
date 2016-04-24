.. _kor-object-table:

Object Table
============

The Object Table tracks all kernel objects.  It is the source of authority: it
can mint any sort of key to any object, determine if keys point to the same
object, and extract brands.

.. warning::

  Applications should carefully consider their use of Object Table keys!  They
  are powerful and should be closely held.  We expect the system to proxy,
  extend, and limit the Object Table API if any of it is exposed to low
  assurance application components.  See :ref:`Rule 6 (enough rope) <rule6>`.

Branding
--------

The brands of Object Table keys should be zero.


Methods
-------

Mint Key (0)
~~~~~~~~~~~~

Mints a key to any object, specified by table index, with an arbitrary (but
valid) brand.  The resulting key will be valid for the object's current
generation, but revoked (like any other key) at the next invalidation of the
object.

.. note::

  Objects may impose validity rules on their brands.  These rules are given in
  the relevant sections of the Kernel Object Reference.  The Object Table will
  not mint a key with an invalid brand.

Call
####

- d0: descriptor
- d1: object index in table
- d2: brand

Reply
#####

- d0: success descriptor
- k1: newly minted key

Exceptions
##########

- ``k.index_out_of_range`` if the index is not within the object table.
- ``k.bad_brand`` if the object vetoes the suggested brand.


Read Key (1)
~~~~~~~~~~~~

Inspects a key and reads out its table index and brand.  This is essentially
the reverse of Mint Key, and is the only way to tell whether two keys refer to
the same object, and the only general way to read out a key's brand.

Call
####

- d0: descriptor
- k0: key

Reply
#####

- d0: success descriptor
- d1: table index
- d2: brand


Get Kind (2)
~~~~~~~~~~~~

Reports the true kind of the object inhabiting a particular object table slot.
Kinds are reported using values from the table below.

==== =========================
Code Kind
==== =========================
0    :ref:`kor-null`
1    :ref:`kor-object-table`
2    :ref:`kor-slot`
3    :ref:`kor-memory`
4    :ref:`kor-context`
5    :ref:`kor-gate`
6    :ref:`kor-reply-gate`
7    :ref:`kor-interrupt`
==== =========================


Call
####

- d0: descriptor
- d1: object index in table

Reply
#####

- d0: success descriptor
- d1: kind code

Exceptions
##########

- ``k.index_out_of_range`` if the index is not within the object table.
