#include "softswitch.hpp"

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message)
{
    
    const packet_t *packet=(const packet_t*)message;
    
    // Map to the device and its vtable
    unsigned deviceIndex=packet->dest.device;
    assert( deviceIndex < ctxt->numDevices );
    DeviceContext *dev=ctxt->devices + deviceIndex;
    const DeviceTypeVTable *vtable=dev->vtable;
    
    // Map to the particular port
    unsigned portIndex=packet->dest.port;
    assert(portIndex < vtable->numInputs);
    const InputPortVTable *port=vtable->inputPorts + portIndex;
    receive_handler_t handler=port->receiveHandler;
            
    // Call the application level handler
    handler(
        ctxt->graphProps,
        dev->properties,
        dev->state,
        0,               // edge properties - how to find them?
        0,               // edge state      - how to find them?
        packet->payload
    );

    softswitch_UpdateRTS(ctxt, dev);
}
