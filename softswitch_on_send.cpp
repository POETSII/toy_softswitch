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
    fprintf(stderr, "softswitch_onSend:   begin\n");    

    
    numTargets=0;
    pTargets=0;
    
    DeviceContext *dev = softswitch_PopRTS(ctxt);
    if(!dev){
        return 0;
    }
    
    const DeviceTypeVTable *vtable=dev->vtable;

    assert(dev->rtsFlags);
    unsigned portIndex=right_most_one(dev->rtsFlags);

    send_handler_t handler=vtable->outputPorts[portIndex].sendHandler;

    fprintf(stderr, "softswitch_onSend:   calling application handler\n");    

    bool doSend=handler(
        ctxt->graphProps,
        dev->properties, 
        dev->state,
        ((packet_t*)message)->payload
    );
    fprintf(stderr, "softswitch_onSend:   application handler done, doSend=%d\n", doSend?1:0);    

    
    if(doSend){
        numTargets=dev->targets[portIndex].numTargets;
        pTargets=dev->targets[portIndex].targets;
    }
    
    fprintf(stderr, "softswitch_onSend:   numTargets=%u, pTargets=%p\n", numTargets, pTargets);    

    fprintf(stderr, "softswitch_onSend:   updating RTS\n");    
    dev->rtsFlags=0;    // Reflect that it is no longer on the RTC list due to the pop
    dev->rtc=false; 
    softswitch_UpdateRTS(ctxt, dev);
    fprintf(stderr, "softswitch_onSend:   rtsFlags=%x\n", dev->rtsFlags);    
    
    fprintf(stderr, "softswitch_onSend:   end\n");    
    
    return vtable->outputPorts[portIndex].messageSize;
}
