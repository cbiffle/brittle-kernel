About Keys
==========

Key Semantics
-------------

To programs, keys appear to be opaque values that can be freely copied but not
directly [#directly]_ created, inspected, or destroyed.  Programs always refer
to keys *indirectly*, by giving the index of a Context key register containing
the desired keys.

Specifically, without holding additional authority, programs can only perform
three operations on a key:

1. Copy the key into a different key register (overwriting the destination).

2. Send a message to the object designated by the key.

3. Receive a message from the object designated by the key.

The send and receive operations are collectively referred to as *invoking* a
key, and can be combined into a "call."  For more details on these operations,
see :ref:`syscalls`.

By design, programs *cannot* do any of the following without holding additional
authority:

- Determine the kind of kernel object designated by a key (e.g. Context vs.
  Gate).

- Determine if two keys designate the same kernel object.

- Determine if two keys give the same rights on their designated objects.


Parts of a Key
--------------

Conceptually, a key combines:

1. A reference to a kernel object, and

2. A set of operations on that object that the key enables.

In implementation, a key is a kernel data structure built from three parts,
which will be explained in more detail shortly:

1. A *brand*, which distinguishes kinds of keys to a particular object from one
   another.

2. A *generation*, which distinguishes keys to successive objects, created in
   the same location, from one another.

3. A pointer to a kernel object.


Brands
------

Every key bears a *brand*, a 64 bit value chosen when the key was created
(*minted*).  The brand is *indelible*: it cannot be altered except by minting a
new key.  It is also *protected*: the kernel keeps track of it on behalf of
programs, and programs cannot generally discover the brand of keys they hold.
When a key is copied (for example, by attaching it to a message) the copies
bear the same brand as the original.

Brands allow two keys to the same object to behave differently.  The meaning of
a brand is entirely up to the object designated by the key.  Kernel objects
specify the meaning of brands so that the System can mint appropriate keys, but
transparent objects (particularly Gates) pass the brand up to programs for
interpretation.  It may encode access permissions, unique client identifiers,
etc.


Generations and Revocation
--------------------------

Each key contains a *generation* and *pointer*.  These are used together to
provide efficient revocation while avoiding dangling pointer bugs.

Whenever a program invokes a key, the kernel compares the key's generation to a
generation field stored inside the object.  If the two match, the invocation
proceeds normally.  If the two *differ*, however, it means the object has been
invalidated since the invoked key was minted.  The kernel rewrites the
offending key to :ref:`kor-null`.

Because the generation is checked before any invocation, the key gets nulled
before any program can *observe* a dangling pointer.  (Copying a key around
doesn't indirect the contained pointer, and thus can't observe a dangling one.)
This guarantees that invalidation *appears* atomic, by lazily nulling any
remaining keys to an invalidated object as they are touched by programs.

Note that the generation is *independent* of the brand.  This means that all
keys to an object, regardless of brand, are revoked simultaneously during
invalidation.

Invalidation is available to programs through the
:ref:`object-table-methods-invalidate` method.  Some objects perform additional
work during invalidation, e.g. to remove themselves from kernel internal
queues; this is described on each object's page in the :ref:`kor`.

.. warning:: The generation field is currently 32 bits wide, which is narrow
  enough that it may roll over during a program's execution.  This is not
  theoretical: once object destruction is exposed, a naughty program could
  repeatedly create and destroy an object, forcing rollover in hours.  There is
  currently no way to detect or correct this.  I'm evaluating two options:
  requiring System intervention at rollover, or expanding generations to 64
  bits.


.. rubric:: Footnotes

.. [#directly] System components can use keys to the :ref:`kor-object-table` to
  create and inspect keys *indirectly*.
