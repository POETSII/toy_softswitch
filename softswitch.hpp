#ifndef softswitch_hpp
#define softswitch_hpp

#include <cstdint>
#include <cassert>

// Some kind of address. Just made this up.
struct address_t
{
    uint32_t thread;  // hardware
    uint16_t device;  // softare
    uint8_t port;     // software
    uint8_t flag;     // ?
};

// A packet. This probably mixes hardware and
// software routing.
struct packet_t
{
    address_t dest;
    address_t source;
    uint8_t payload[48];
};

typedef uint32_t (*ready_to_send_handler_t)(
    const void *graphProps,
    const void *devProps,
    void *devState
);

typedef bool (*send_handler_t)(
    const void *graphProps,
    const void *devProps,
    void *devState,
    void *message
);

typedef void (*receive_handler_t)(
    const void *graphProps,
    const void *devProps,
    void *devState,
    const void *edgeProps,
    void *edgeState,
    const void *
);

typedef void (*compute_handler_t)(
    const void *graphProps,
    const void *devProps,
    void *devState
);

struct InputPortVTable
{
    receive_handler_t receiveHandler;
    // Note currently used, but we probably need some information
    // about whether it has properties/state?
    // uint16_t propertiesSize;
    // uint16_t stateSize;
};

struct OutputPortVTable
{
    send_handler_t sendHandler;
};

// Gives access to the code associated with each device
struct DeviceTypeVTable
{
    ready_to_send_handler_t readyToSendHandler;
    unsigned numInputs;
    OutputPortVTable *outputPorts;
    unsigned numOutputs;
    InputPortVTable *inputPorts;
    compute_handler_t computeHandler;
};

// Gives the distribution list when sending from a particular port
// Read-only. This is likely to be unique to each port, but it may be possible
// to share.
struct OutputPortTargets
{
    unsigned numTargets;
    const address_t *targets;
};

//! Context is fixed size. Points to varying size properties and state
struct DeviceContext
{
    // These parts are fixed, and will never change
    const DeviceTypeVTable *vtable;
    const void *properties;
    void *state;
    unsigned index;
    OutputPortTargets *targets;
    
    uint32_t rtsFlags;
    bool rtc;
    
    DeviceContext *prev;
    DeviceContext *next;
};

/*! Contains information about what this thread is managing.
    Some parts are read-only and can be shared (e.g. device vtables, graph properties) 
    Other parts are read-only but probably not shared (device properties, address lists)
    Some parts are read-write, and so must be private (e.g. the devices array, rtsHead, ...)
*/
struct PThreadContext
{
    // Read-only parts
    const void *graphProps; // Application-specific graph properties (read-only)
    
    unsigned nVTables;      // Number of distinct device types available
    const DeviceTypeVTable *vtables; // VTable structure for each device type (read-only, could be shared with other pthread contexts)
  
    unsigned numDevices;    // Number of devices this thread is managing
    DeviceContext *devices; // Fixed-size contexs for each device (must be private to thread)
    
    // Mutable parts
    DeviceContext *rtsHead;
    DeviceContext *rtsTail;
    
    uint32_t rtcChecked; // How many we have checked through since last seeing an RTC
                         // If rtcChecked >= numDevices, we know there is no-one who wants to compute
    uint32_t rtcOffset;  // Where we are in the checking list (to make things somewhat fair)
};

extern "C" void softswitch_UpdateRTS(
    PThreadContext *pCtxt, DeviceContext *dCtxt
);

extern "C" DeviceContext *softswitch_PopRTS(PThreadContext *pCtxt);

extern "C" bool softswitch_IsRTSReady(PThreadContext *pCtxt);

#endif
