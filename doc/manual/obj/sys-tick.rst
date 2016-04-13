.. _kor-systick:

SysTick
=======

The SysTick object provides unprivileged access to the ARMv7-M SysTick Timer,
which is located on the Private Peripheral Bus and cannot be directly accessed
by unprivileged code --- even using the MPU.

This is the closest thing to a "driver" you'll find in Brittle at the moment.


Initialization Properties
-------------------------

None.


Branding
--------

SysTick key brands should be zero.


Methods
-------

SysTick implements all methods defined for :ref:`Interrupt <interrupt-methods>`,
plus the following.


Read Register (3)
~~~~~~~~~~~~~~~~~

Reads a register from the SysTick timer.  Valid registers:

- 0: CSR
- 1: RVR
- 2: CVR
- 3: CALIB

Call
####

- d0: descriptor
- d1: register number

Reply
#####

- d0: success descriptor
- d1: register value

Exceptions
##########

- ``k.index_out_of_range`` if the register number is not one of the values
  listed above.


Write Register (4)
~~~~~~~~~~~~~~~~~~

Writes a register in the SysTick timer.  Valid registers:

- 0: CSR
- 1: RVR
- 2: CVR
- 3: CALIB

Call
####

- d0: descriptor
- d1: register number
- d2: new value.

Reply
#####

- d0: success descriptor

Exceptions
##########

- ``k.index_out_of_range`` if the register number is not one of the values
  listed above.


