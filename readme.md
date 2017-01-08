Overview
========

This is a toy softswitch designed to be
compatible with the Tinsel API underneath,
and the application model on top. 

It appears to me to follow the Tinsel API
(apart from message format), and supports the
semantics of the application graphs.

Features are:

- Fixed memory footprint (no dynamic allocation)

- All configuration is accessed via a single
  pointer to a struct (the PThreadContext).

- All IO is via the Tinsel API

- Fair constant-time scheduling of send threads

- Can send messages to multiple outputs without
  an r-device, and without blocking incoming messages.

The most interesting part is probably in
[softswitch_main.cpp](src/softswitch/softswitch_main.cpp), which is the main
driver loop. It is designed so that all `tinsel_*` cores
are located in `softswitch_main`: all other parts are
pure software.

It compiles and runs, and (ignoring very minor Tinsel API
tweaks) the softswitch and application code would run on
actual hardware as-is (I think).

Tinsel API
==========

There is a simulation-only version of the Tinsel mbox API included,
which uses Unix domain data-gram sockets to send messages around,
and multiple processes to represents the multiple POETS threads.



Data Structures
===============

Threads are configured with a combination of read-only and read-write
data, which tells them about the devices they are managing,
as well as the properties of those devices. All data-structures
require a fixed amount of size, and could be linked in as a
combination of read-only and read-write data-sections.

There is the concept of a "vtable", which is pretty much
the same as a C++ vtable, but here is used to point towards
input/output handlers and meta-data associated with them.

An overview of the vtable structure is:

    DEVICE_TYPES_VTABLES[...] : DeviceVTable
    |
    0 : DeviceTypeVTable
        |
        +-read_to_send_handler : function which says if this kind of device wants to send
        |
        +-OUTPUT_COUNT_xxx : the number of outputs for the device type xxx
        |
        +-OUTPUT_VTABLES_xxx : pointer to an array of output vtables
        |
        +
