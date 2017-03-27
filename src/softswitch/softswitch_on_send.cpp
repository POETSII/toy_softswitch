#include "softswitch.hpp"

#include <cstdio>

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
/*! \param numTargets Receives the number o f addresses to send the packet to
    \param pTargets If numTargets>0, this will be pointed at an array with numTargets elements
    \retval Size of message in bytes
*/
extern "C" unsigned softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets)
{
    softswitch_softswitch_log(3, "softswitch_onSend : begin");    
    
    numTargets=0;
    pTargets=0;
    
    DeviceContext *dev = softswitch_PopRTS(ctxt);
    if(!dev){
        return 0;
    }
    
    const DeviceTypeVTable *vtable=dev->vtable;

    assert(dev->rtsFlags);
    unsigned portIndex=right_most_one(dev->rtsFlags);
    
    softswitch_softswitch_log(4, "softswitch_onSend : device=%08x:%04x, rtsFlags=%x, selected=%u", ctxt->threadId, dev->index,  dev->rtsFlags, portIndex);    

    send_handler_t handler=vtable->outputPorts[portIndex].sendHandler;

    softswitch_softswitch_log(4, "softswitch_onSend : calling application handler");    

    ctxt->lamport++;
    
    // Needed for handler logging
    ctxt->currentDevice=dev->index;
    ctxt->currentHandlerType=2;
    ctxt->currentPort=portIndex;

    bool doSend=handler(
        ctxt->graphProps,
        dev->properties, 
        dev->state,
        ((packet_t*)message)->payload
    );

    ctxt->currentHandlerType=0;
    
    softswitch_softswitch_log(4, "softswitch_onSend : application handler done, doSend=%d", doSend?1:0);    

    
    if(doSend){
        numTargets=dev->targets[portIndex].numTargets;
        pTargets=dev->targets[portIndex].targets;
        ((packet_t*)message)->source.thread=ctxt->threadId;
        ((packet_t*)message)->source.device=dev->index;
        ((packet_t*)message)->source.port=portIndex;
        ((packet_t*)message)->lamport=ctxt->lamport;
    }
    
    softswitch_softswitch_log(4, "softswitch_onSend : numTargets=%u, pTargets=%p", numTargets, pTargets);    

    softswitch_softswitch_log(4, "softswitch_onSend : updating RTS");    
    dev->rtsFlags=0;    // Reflect that it is no longer on the RTC list due to the pop
    dev->rtc=false; 
    softswitch_UpdateRTS(ctxt, dev);
    softswitch_softswitch_log(4, "softswitch_onSend : rtsFlags=%x", dev->rtsFlags);    
    
    softswitch_softswitch_log(3, "softswitch_onSend : end");    
    
    return vtable->outputPorts[portIndex].messageSize;
}
