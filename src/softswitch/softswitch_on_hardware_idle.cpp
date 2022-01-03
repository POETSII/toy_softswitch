#include "softswitch.hpp"
#include "softswitch_hostmessaging.hpp"

extern "C" void softswitch_on_hardware_idle(PThreadContext *ctxt)
{
    softswitch_softswitch_log(2, "softswitch_on_hardware_idle : begin");

    ctxt->rtsHead=0;
    ctxt->rtsTail=0;

    for(unsigned i=0; i<ctxt->numDevices; i++){
        DeviceContext *dev=ctxt->devices+i;
        assert(dev->rtsFlags==0);
        assert(!rts_is_on_list(ctxt, dev));

        const DeviceTypeVTable *vtable=dev->vtable;

        on_hardware_idle_handler_t handler = vtable->onHardwareIdleHandler;
        if(handler){
          softswitch_softswitch_log(3, "softswitch_on_hardware_idle : calling hardware idle handler for %s.", dev->id);

          // Needed for handler logging
          ctxt->currentDevice=i;
          ctxt->currentHandlerType=5; // Identify this as init handler
          //ctxt->currentPin=0;

          handler(ctxt->graphProps, dev->properties, dev->state);

          softswitch_UpdateRTS(ctxt, dev);
        }
        // TODO : try to drain host buffer messages here? Only matters if lots of devices per thread
    }

    ctxt->currentHandlerType=0;

    softswitch_softswitch_log(3, "softswitch_on_hardware_idle : end, rtsHead=%d.", ctxt->rtsHead);
}
