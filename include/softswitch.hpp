#ifndef softswitch_hpp
#define softswitch_hpp

#include <cstdint>
#include <cassert>

// Some kind of address. Just made this up.
struct address_t
{
    // Working round bug in gcc before gcc5.
    // Previously default aggregate init was used, but that doesn't allow
    // default init of _flag to 0.
    // Added constructor to get the init.
    address_t(uint32_t _thread, uint16_t _device, uint8_t _port, uint8_t _flag=0)
        : thread(_thread)
        , device(_device)
        , port(_port)
        , flag(_flag)
    {}
    
    uint32_t thread;    // hardware
    uint16_t device;    // softare
    uint8_t port;       // software
    uint8_t flag; //=0; // software
};

// A packet. This probably mixes hardware and
// software routing.
struct packet_t
{
    address_t dest;
    address_t source;
    uint32_t lamport; // lamport clock
    uint8_t payload[];
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
    const void *message
);

typedef void (*compute_handler_t)(
    const void *graphProps,
    const void *devProps,
    void *devState
);

struct InputPortVTable
{
    receive_handler_t receiveHandler;
    unsigned messageSize;
    // If both properties and state are zero, we never have to look
    // them up...
    uint16_t propertiesSize;
    uint16_t stateSize;
    const char *name;
};

struct OutputPortVTable
{
    send_handler_t sendHandler;
    unsigned messageSize;
    const char *name;
};

// Gives access to the code associated with each device
struct DeviceTypeVTable
{
    ready_to_send_handler_t readyToSendHandler;
    unsigned numOutputs;
    OutputPortVTable *outputPorts;
    unsigned numInputs;
    InputPortVTable *inputPorts;
    compute_handler_t computeHandler;
    const char *id; // Unique id of the device type
};

// Gives the distribution list when sending from a particular port
// Read-only. This is likely to be unique to each port, but it may be possible
// to share.
struct OutputPortTargets
{
    unsigned numTargets;
    address_t *targets;
};

struct InputPortBinding
{
    address_t source;
    const void *edgeProperties;
    void *edgeState;
};

// Allows us to bind incoming messages to the appropriate edge properties
/* The properties will be stored in order of address then port. This would
   be much better done by a hash-table or something. Or possibly embedding
    the destination properties into the message?
*/
struct InputPortSources
{
    unsigned numSources;  // This will be null if the input has properties/state
    InputPortBinding *sourceBindings; // This will be null if the input had no properties or state
};

//! Context is fixed size. Points to varying size properties and state
struct DeviceContext
{
    // These parts are fixed, and will never change
    DeviceTypeVTable *vtable;
    const void *properties;
    void *state;
    unsigned index;
    OutputPortTargets *targets;  // One entry per output port
    InputPortSources *sources;   // One entry per input port
    const char *id;              // unique id of the device

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

    unsigned threadId;

    const void *graphProps; // Application-specific graph properties (read-only)


    unsigned numVTables;      // Number of distinct device types available
    DeviceTypeVTable *vtables; // VTable structure for each device type (read-only, could be shared with other pthread contexts)

    unsigned numDevices;    // Number of devices this thread is managing
    DeviceContext *devices; // Fixed-size contexs for each device (must be private to thread)

    // Mutable parts
    uint32_t lamport; // clock

    DeviceContext *rtsHead;
    DeviceContext *rtsTail;

    uint32_t rtcChecked; // How many we have checked through since last seeing an RTC
                         // If rtcChecked >= numDevices, we know there is no-one who wants to compute
    uint32_t rtcOffset;  // Where we are in the checking list (to make things somewhat fair)

    int logLevel;        // Controls how much output is printed

    // This is used by the softswitch to track which device (if any) is active, so that we know during things like handler_log
    uint32_t currentDevice; 
    int currentHandlerType;   // 0 = None, 1 = Recv, 2 = Send
    uint32_t currentPort; // Index of the port (for recv and send)
};

extern "C" void softswitch_UpdateRTS(
    PThreadContext *pCtxt, DeviceContext *dCtxt
);

extern "C" DeviceContext *softswitch_PopRTS(PThreadContext *pCtxt);

extern "C" bool softswitch_IsRTSReady(PThreadContext *pCtxt);

//! Allows logging from within a device handler
extern "C" void softswitch_handler_log(int level, const char *msg, ...);


//! Gives the total number of threads in the application
extern "C" unsigned softswitch_pthread_count;

//! An array of softswitch_pthread_count entries
/*! Bit of a hack, as it means every thread has a copy
    of the whole thing. */
extern "C" PThreadContext softswitch_pthread_contexts[];

extern "C" void softswitch_main();


#endif
