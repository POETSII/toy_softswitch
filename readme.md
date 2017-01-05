This is a toy softswitch designed to be
compatible with the Tinsel API underneath,
and the application model on top. It compiles,
and 

Features are:

- Fixed memory footprint (no dynamic allocation)

- All configuration is accessed via a single
  pointer to a struct (the PThreadContext).

- All IO is via the Tinsel API

- Fair constant-time scheduling of send threads

- Can send messages to multiple outputs without
  an r-device, and without blocking incoming messages.

The most interesting part is probably in [softswitch_main.cpp],
which is the main driver loop.
