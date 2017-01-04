#include "softswitch.hpp"

static unsigned right_most_one(uint32_t x)
{
    assert(x);
    
    unsigned index=0;
    while(!(x&1)){
        index++;
        x=x>>1;
    }
    
    return index;
}

//! Prepare a message on the given buffer
/*! \param numTargets Receives the number of addresses to send the packet to
    \param pTargets If numTargets>0, this will be pointed at an array with numTargets elements
*/
extern "C" void softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets)
{
    numTargets=0;
    pTargets=0;
    
    DeviceContext *dev = P_PopRTS(ctxt);
    if(!dev){
        return;
    }
    
    const DeviceTypeVTable *vtable=dev->vtable;

    assert(dev->rtsFlags);
    unsigned portIndex=right_most_one(dev->rtsFlags);

    send_handler_t handler=vtable->outputPorts[portIndex].sendHandler;

    bool doSend=handler(
        ctxt->graphProps,
        dev->properties, 
        dev->state,
        ((packet_t*)message)->payload
    );
    if(doSend){
        numTargets=dev->targets[portIndex].numTargets;
        pTargets=dev->targets[portIndex].targets;
    }
      
    softswitch_UpdateRTS(ctxt, dev);
}
