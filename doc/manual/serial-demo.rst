Case Study: The Serial Demo
===========================

The serial demo is a simple demo application for the STM32F407 microcontroller,
distributed with the Brittle sources in the ``demo/ascii`` directory.

At boot, it repeatedly transmits the printable subset of the ASCII character
set using a UART, at 115200 bps on pin PA2.

It demonstrates the following:

- How to write applications that directly target Brittle, without a system
  layer like FreeRTOS.

- How to structure a multi-process application with isolation of authority
  (though not memory isolation at the moment).

- How to write an interrupt-driven hardware peripheral driver using Brittle's
  :ref:`kor-interrupt` object.

.. note:: Since there isn't a convenient library for applications targeting
  Brittle yet, this demo has to do a lot by hand.  This will change as parts
  get factored out into a library.


Tasks
-----

At a high level, the demo consists of three tasks.  Each task gets its own
:ref:`kor-context`, and thus its own virtual copy of the processor and Key
Registers to hold authority.

The tasks are:

Driver
  Manages the transmit side of the UART, responding to Client requests and
  interrupts.  Holds keys that allow it to receive requests and service the
  :ref:`kor-interrupt` object.

Client
  Invokes the UART driver over IPC to send an infinite stream of ASCII
  characters.  Holds a single key that lets it submit UART transmit requests to
  the driver.

Idle
  Puts the processor into low power mode when neither of the other tasks has
  any work to do --- i.e. when waiting for a transmit-complete interrupt.
  Holds no keys, so it cannot interfere with the other tasks, except possibly
  by scribbling over their memory, since the tasks aren't currently isolated in
  memory.


Driver Operation
----------------

The UART driver is implemented in ``demo/drv/uart/driver.cc``, and provides a
client library in ``demo/drv/uart/client.cc``.

On task startup, the driver initializes the UART, along with the shared
resources (clocks and reset lines) required to make the UART work.  It then
enables the interrupt and enters a service loop.

The service loop repeatedly does the following:

- Receives any waiting transmit requests.

- Loads data into the UART transmit register.

- Blocks waiting for a message from the interrupt.

- Repeats.

The driver services *two* :ref:`Gates <kor-gate>`: one to receive requests from
the Client task, and one to receive interrupt messages.  At any given time, the
driver is receiving from *one* of the two Gates.  This provides flow control:
while the UART is busy, Client requests will block, since the driver is only
listening for interrupt messages.  When the UART is idle, the driver only
listens for client messages.

This lets the IPC mechanism serve as a sort of natural buffer.  Clients
attempting to transmit will block if the UART is busy, and their requests will
be processed as it becomes available (in priority order).


Startup
-------

The demo is started directly by the kernel, meaning it wakes up in the
:ref:`boot environment <boot>`.  This means it initially holds an
:ref:`kor-object-table` key granting tremendous authority.

The first goal is to shed that authority --- but only after using it to set up
the rest of the demo.

The demo startup routine (in ``demo/ascii/main.cc``) contains a basic memory
allocator, which is initially handed all of the system's RAM by the boot
process.  It subdivides RAM into small blocks, and then donates them to the
kernel to create new objects, using :ref:`kor-memory`'s
:ref:`memory-method-become` method.

The created objects include:

- The :ref:`kor-interrupt` object used to service interrupts from USART2.

- The :ref:`kor-gate` used to transfer interrupt messages to the driver task,
  and the Gate used by the client to communicate with the driver.

- The :ref:`Contexts <kor-context>` representing the client and idle tasks.

After generating and wiring these objects, the startup routine then mints a
:ref:`kor-memory` key giving access to USART2's registers, and loads it into
its own MPU region registers.  It then calls the driver's entry point,
effectively becoming the driver task.





