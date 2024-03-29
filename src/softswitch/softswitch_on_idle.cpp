#include "softswitch.hpp"


extern "C" bool softswitch_onIdle(PThreadContext *ctxt)
{
    DeviceContext *dev=0;

    if(ctxt->rtcChecked >= ctxt->numDevices){
        return false; // We already did a complete loop and there was nothing
    }

    // Controls how long we spend looking for idle work
    // between checking if we can send/recv
    const unsigned IDLE_SWEEP_CHUNK_SIZE = 32;

    unsigned i;
    for(i=0; i<IDLE_SWEEP_CHUNK_SIZE;i++){
        dev=ctxt->devices + ctxt->rtcOffset;

        ctxt->rtcOffset++;
        ctxt->rtcChecked++;
        if(ctxt->rtcOffset >= ctxt->numDevices){
            ctxt->rtcOffset=0;
        }

        if( dev->rtsFlags&0x80000000 ){
            break;
        }
    }

    if(i==IDLE_SWEEP_CHUNK_SIZE){
        return true; // Didn't find one this time, but there might still be one
    }

    const DeviceTypeVTable *vtable=dev->vtable;

    vtable->computeHandler(
        ctxt->graphProps,
        dev->properties,
        dev->state
    );

    assert( ((dev->rtsFlags&0x7FFFFFFFul)!=0) == rts_is_on_list(ctxt, dev));
    softswitch_UpdateRTS(ctxt, dev);

    return true;
}
