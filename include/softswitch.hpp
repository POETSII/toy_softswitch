#ifndef softswitch_hpp
#define softswitch_hpp

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

// Some kind of address. Just made this up.
#pragma pack(push,1)
typedef struct _address_t
{
    uint32_t thread;    // hardware
    uint16_t device;    // softare
    uint8_t pin;       // software
    uint8_t flag; //=0; // software
}address_t;
#pragma pack(pop)

// A packet. This probably mixes hardware and
// software routing.
#pragma pack(push,1)
typedef struct _packet_t
{
  address_t dest;
  address_t source;
  uint32_t size;  // total size, including header
}packet_t;
#pragma pack(pop)

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

typedef struct _InputPinVTable
{
    receive_handler_t receiveHandler;
    unsigned messageSize;
    // If both properties and state are zero, we never have to look
    // them up...
    uint16_t propertiesSize;
    uint16_t stateSize;
    const char *name;
}InputPinVTable;

typedef struct _OutputPinVTable
{
    send_handler_t sendHandler;
    unsigned messageSize;
    const char *name;
}OutputPinVTable;

// Gives access to the code associated with each device
typedef struct _DeviceTypeVTable
{
    ready_to_send_handler_t readyToSendHandler;
    unsigned numOutputs;
    const OutputPinVTable *outputPins;
    unsigned numInputs;
    const InputPinVTable *inputPins;
    compute_handler_t computeHandler;
    const char *id; // Unique id of the device type
}DeviceTypeVTable;

// Gives the distribution list when sending from a particular pin
// Read-only. This is likely to be unique to each pin, but it may be possible
// to share.
typedef struct _OutputPinTargets
{
    unsigned numTargets;
    address_t *targets; 
}OutputPinTargets;

typedef struct _InputPinBinding
{
    address_t source;
    const void *edgeProperties; // On startup this is zero or a byte offset in global properties array
    void *edgeState;            // On startup this is zero or a byte offset into global state array
}InputPinBinding;

// Allows us to bind incoming messages to the appropriate edge properties
/* The properties will be stored in order of address then pin. This would
   be much better done by a hash-table or something. Or possibly embedding
    the destination properties into the message?
*/
typedef struct _InputPinSources
{
    unsigned numSources;  // This will be null if the input has properties/state
    const InputPinBinding *sourceBindings; // This will be null if the input had no properties or state
}InputPinSources;

//! Context is fixed size. Points to varying size properties and state
typedef struct _DeviceContext
{
    // These parts are fixed, and will never change
    const DeviceTypeVTable *vtable;
    const void *properties;
    void *state;
    unsigned index;
    const OutputPinTargets *targets;  // One entry per output pin
    const InputPinSources *sources;   // One entry per input pin
    const char *id;              // unique id of the device

    uint32_t rtsFlags;
    bool rtc;

    struct _DeviceContext *prev;
    struct _DeviceContext *next;
}DeviceContext;

/*! Contains information about what this thread is managing.
    Some parts are read-only and can be shared (e.g. device vtables, graph properties)
    Other parts are read-only but probably not shared (device properties, address lists)
    Some parts are read-write, and so must be private (e.g. the devices array, rtsHead, ...)
*/
typedef struct _PThreadContext
{
    // Read-only parts

    unsigned threadId;

    const void *graphProps; // Application-specific graph properties (read-only)


    unsigned numVTables;      // Number of distinct device types available
    const DeviceTypeVTable *vtables; // VTable structure for each device type (read-only, could be shared with other pthread contexts)

    unsigned numDevices;    // Number of devices this thread is managing
    DeviceContext *devices; // Fixed-size contexs for each device (must be private to thread)

    // Mutable parts
    uint32_t lamport; // clock

    DeviceContext *rtsHead;
    DeviceContext *rtsTail;

    uint32_t rtcChecked; // How many we have checked through since last seeing an RTC
                         // If rtcChecked >= numDevices, we know there is no-one who wants to compute
    uint32_t rtcOffset;  // Where we are in the checking list (to make things somewhat fair)

    int applLogLevel;        // Controls how much output is printed from the application
    int softLogLevel;        // Controls how much output is printed from the softswitch
    int hardLogLevel;        // Controls how much output is printed from hardware

    // This is used by the softswitch to track which device (if any) is active, so that we know during things like handler_log
    uint32_t currentDevice; 
    int currentHandlerType;   // 0 = None, 1 = Recv, 2 = Send
    uint32_t currentPin; // Index of the pin (for recv and send)
    
    // If true, then the softswitch must go through and turn relative
    // pointers during init. All relevant pointers should only be read/written
    // by this thread, so it should be possible to do it on a per-thread basis.
    // Things to patch are:
    // - deviceProperties -> softswitch_pthread_global_properties+(uintptr_t)deviceProperties
    // - deviceState -> softswitch_pthread_global_state+(uintptr_t)deviceState
    // - edgeProperties -> softswitch_pthread_global_properties+(uintptr_t)edgeProperties
    // - edgeState -> softswitch_pthread_global_state+(uintptr_t)edgeState
    int pointersAreRelative;
}PThreadContext;

void softswitch_UpdateRTS(
    PThreadContext *pCtxt, DeviceContext *dCtxt
);

DeviceContext *softswitch_PopRTS(PThreadContext *pCtxt);

bool softswitch_IsRTSReady(PThreadContext *pCtxt);


#ifndef POETS_MAX_LOGGING_LEVEL
#define POETS_MAX_LOGGING_LEVEL 10
#endif

//! Allows logging from within a device handler
#ifndef POETS_DISABLE_LOGGING
#define softswitch_handler_log(level, ...) \
  if(level <= POETS_MAX_LOGGING_LEVEL){ softswitch_handler_log_impl(level, __VA_ARGS__); }
#else
#define softswitch_handler_log(level, ...) \
    ((void)0)
#endif
void softswitch_handler_log_impl(int level, const char *msg, ...);

//! Used by a device to indicate everything is finished
void softswitch_handler_exit(int code);

//! Allows logging from within the softswitch
#ifndef POETS_DISABLE_LOGGING
#define softswitch_softswitch_log(level, ...) \
  if(level <= POETS_MAX_LOGGING_LEVEL){ softswitch_softswitch_log_impl(level, __VA_ARGS__); }
#else
#define softswitch_softswitch_log(level, ...) \
    ((void)0)
#endif
void softswitch_softswitch_log_impl(int level, const char *msg, ...);


// Send a key-value pair back to the host.
void softswitch_handler_export_key_value(uint32_t key, uint32_t value);


//! Gives the total number of threads in the application
extern unsigned softswitch_pthread_count;

//! An array of softswitch_pthread_count entries
/*! Bit of a hack, as it means every thread has a copy
    of the whole thing. */
extern PThreadContext softswitch_pthread_contexts[];

extern const DeviceTypeVTable softswitch_device_vtables[];

//! Array of all property BLOBs used in the program (graph, device, edge)
extern const uint8_t softswitch_pthread_global_properties[];

//! Array of all state BLOBs used in the program (device, edge)
extern uint8_t softswitch_pthread_global_state[];

//! Array of all output pin (len,pAddress) pairs
extern const OutputPinTargets softswitch_pthread_output_pin_targets[];

void softswitch_main();

#ifdef __cplusplus
};
#endif

#endif
