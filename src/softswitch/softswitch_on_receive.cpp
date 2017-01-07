#include "softswitch.hpp"

#include <cstdio>

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message)
{
    fprintf(stderr, "softswitch_onReceive: begin\n");
    
    const packet_t *packet=(const packet_t*)message;
    
    // Map to the device and its vtable
    unsigned deviceIndex=packet->dest.device;
    fprintf(stderr, "softswitch_onReceive:   finding vtable, device inst index=%u\n", deviceIndex);
    assert( deviceIndex < ctxt->numDevices );
    DeviceContext *dev=ctxt->devices + deviceIndex;
    const DeviceTypeVTable *vtable=dev->vtable;
        
    // Map to the particular port
    unsigned portIndex=packet->dest.port;
    fprintf(stderr, "softswitch_onReceive:   finding port, port name index=%u\n", portIndex);
    assert(portIndex < vtable->numInputs);
    const InputPortVTable *port=vtable->inputPorts + portIndex;
    receive_handler_t handler=port->receiveHandler;
    
            
    // Call the application level handler
    fprintf(stderr, "softswitch_onReceive:   begin application handler, packet=%p, payload=%p\n", packet, packet->payload);
    handler(
        ctxt->graphProps,
        dev->properties,
        dev->state,
        0,               // edge properties - how to find them?
        0,               // edge state      - how to find them?
        packet->payload
    );
    fprintf(stderr, "softswitch_onReceive:   end application handler\n");

    fprintf(stderr, "softswitch_onReceive:   updating RTS\n");
    softswitch_UpdateRTS(ctxt, dev);
    fprintf(stderr, "softswitch_onReceive:   new rts=%x\n", dev->rtsFlags);
    
    fprintf(stderr, "softswitch_onReceive: end\n");
}
