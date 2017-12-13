#include "softswitch.hpp"
#include "tinsel_api.hpp"

#include <cstdio>

#ifdef SOFTSWITCH_ENABLE_PROFILE
#include "softswitch_perfmon.hpp"
#endif

extern "C" void tinsel_puts(const char *);

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

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    volatile unsigned tstart = tinsel_CycleCount();
    #endif

    softswitch_softswitch_log(3, "softswitch_onSend : begin");    
    
    numTargets=0;
    pTargets=0;

    DeviceContext *dev = softswitch_PopRTS(ctxt);
    if(!dev){
        return 0;
    }

    
    const DeviceTypeVTable *vtable=dev->vtable;

    assert(dev->rtsFlags);
    unsigned pinIndex=right_most_one(dev->rtsFlags);

    
    softswitch_softswitch_log(4, "softswitch_onSend : device=%08x:%04x=%s, rtsFlags=%x, selected=%u", ctxt->threadId, dev->index,  dev->id, dev->rtsFlags, pinIndex);    


    send_handler_t handler=vtable->outputPins[pinIndex].sendHandler;

    softswitch_softswitch_log(4, "softswitch_onSend : calling application handler");    

    ctxt->lamport++;
    
    // Needed for handler logging
    ctxt->currentDevice=dev->index;
    ctxt->currentHandlerType=2;
    ctxt->currentPin=pinIndex;

    char *payload=(char*)((packet_t*)message+1);

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    volatile unsigned hstart = tinsel_CycleCount();
    #endif

    bool doSend=handler(
        ctxt->graphProps,
        dev->properties, 
        dev->state,
	payload
    );

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    volatile unsigned hend = tinsel_CycleCount();
    ctxt->sendHandler_cnt += deltaCycles(hstart, hend);
    #endif

    ctxt->currentHandlerType=0;
    
    softswitch_softswitch_log(4, "softswitch_onSend : application handler done, doSend=%d", doSend?1:0);

    uint32_t messageSize=vtable->outputPins[pinIndex].messageSize;

    if(doSend){
        numTargets=dev->targets[pinIndex].numTargets;
        pTargets=dev->targets[pinIndex].targets;
        ((packet_t*)message)->source.thread=ctxt->threadId;
        ((packet_t*)message)->source.device=dev->index;
        ((packet_t*)message)->source.pin=pinIndex;
        ((packet_t*)message)->size=messageSize;

        softswitch_softswitch_log(4, "softswitch_onSend : source = %x:%x:%x", ctxt->threadId, dev->index, pinIndex);    
    }

    uint32_t payloadSize=messageSize - sizeof(packet_t);
    for(uint32_t i=0; i<payloadSize; i++){
      softswitch_softswitch_log(5, "softswitch_onSend :   payload[%u] = %u", i, (uint8_t)payload[i]);
    }
    
    softswitch_softswitch_log(4, "softswitch_onSend : messageSize=%u, numTargets=%u, pTargets=%p", messageSize, numTargets, pTargets);    

    softswitch_softswitch_log(4, "softswitch_onSend : updating RTS");    
    dev->rtsFlags=0;    // Reflect that it is no longer on the RTC list due to the pop
    dev->rtc=false; 
    softswitch_UpdateRTS(ctxt, dev);
    softswitch_softswitch_log(4, "softswitch_onSend : rtsFlags=%x", dev->rtsFlags);    
    
    softswitch_softswitch_log(3, "softswitch_onSend : end");    
    
    #ifdef SOFTSWITCH_ENABLE_PROFILE
    ctxt->sendOverhead_cnt += deltaCycles(tstart, tinsel_CycleCount());
    #endif

    return messageSize;
}
