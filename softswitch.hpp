#ifndef softswitch_hpp
#define softswitch_hpp

#include <cstdint>
#include <cassert>

struct address_t
{
    uint32_t thread;
    uint16_t device;
    uint8_t port;
    uint8_t flag;
};

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
    uint16_t propertiesSize;
    uint16_t stateSize;
};

struct OutputPortVTable
{
    send_handler_t sendHandler;
    uint32_t destAddress; // Send it somewhere?
};

struct DeviceTypeVTable
{
    ready_to_send_handler_t readyToSendHandler;
    unsigned numInputs;
    OutputPortVTable *outputPorts;
    unsigned numOutputs;
    InputPortVTable *inputPorts;
    compute_handler_t computeHandler;
};

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

struct PThreadContext
{
    // Read-only parts
    const void *graphProps;
    unsigned nVTables;
    const DeviceTypeVTable *vtables;
    unsigned numDevices;
    DeviceContext *devices;
    
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
