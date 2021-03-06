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

.. _object-table-methods-mint-key:

Mint Key (1)
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

- d0: object index in table
- d1: brand low bits
- d2: brand high bits

Reply
#####

No data.

- k1: newly minted key

Exceptions
##########

- ``k.index_out_of_range`` if the index is not within the object table.
- ``k.bad_brand`` if the object vetoes the suggested brand.


Read Key (2)
~~~~~~~~~~~~

Inspects a key and reads out its table index and brand.  This is essentially
the reverse of Mint Key, and is the only way to tell whether two keys refer to
the same object, and the only general way to read out a key's brand.

Call
####

No data.

- k1: key

Reply
#####

- d0: table index
- d1: brand low bits
- d2: brand high bits


Get Kind (3)
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
6    :ref:`kor-interrupt`
==== =========================


Call
####

- d0: object index in table

Reply
#####

- d0: kind code

Exceptions
##########

- ``k.index_out_of_range`` if the index is not within the object table.


.. _object-table-methods-invalidate:

Invalidate (4)
~~~~~~~~~~~~~~

Advances an object's generation number, causing all outstanding keys to become
invalid.

After invalidation, you can use :ref:`object-table-methods-mint-key` to produce
a new, valid key.

.. warning:: Generation numbers are currently relatively small (32 bits), so it
  is possible to use this operation enough that the Generation rolls over.
  This could cause previously invalidated keys to become valid again, unless
  some System component has scavenged such keys.

Call
####

- d0: object index in table
- d1: rollover permitted flag (0: exception on rollover; 1: allow)

Reply
#####

Empty.

Exceptions
##########

- ``k.index_out_of_range`` if the index is not within the object table.
- ``k.causality`` if rollover would occur but has not been permitted.
