.. _kor-reply-gate:

Reply Gate
==========

Reply Gates are a special variety of Gates, used to handle replies to
call-style IPC from Contexts.

Each Context has an associated Reply Gate.


Branding
--------

Reply Gate brands are used as an extension of the Object Table's generation
number.  Any key used to send to a Reply Gate is immediately atomically
revoked; if its brand happens to match the token stored in the Reply Gate, its
message will be passed on.


Methods
-------

Reply Gates have no inherent methods.
