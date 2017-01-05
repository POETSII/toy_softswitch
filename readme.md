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

Missing features are:

- Edge properties and edge state (synapse properties)

The most interesting part is probably in
[softswitch_main.cpp](softswitch_main.cpp), which is the main
driver loop. It is designed so that all `T_*` cores
are located in `softswitch_main`: all other parts are
pure software.

It compiles, but has never been run. What would be needed are:
- An implementation of the Tinsel API
- The structures in the PThreadContext to be
  initialised with the handlers, topology, ...
  
  
