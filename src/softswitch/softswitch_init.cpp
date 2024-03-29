#include "softswitch.hpp"
#include "softswitch_hostmessaging.hpp"

extern "C" int strcmp ( const char * str1, const char * str2 );

extern "C" void tinsel_puts(const char *m);

#ifdef SOFTSWITCH_ENABLE_PROFILE
void zeroProfileCounters(PThreadContext *ctxt)
{
    ctxt->thread_cycles_tstart = 0;
    ctxt->thread_cycles = 0;
    ctxt->send_blocked_cycles = 0;
    ctxt->recv_blocked_cycles = 0;
    ctxt->idle_cycles = 0;
    ctxt->perfmon_cycles = 0;
    ctxt->send_cycles = 0;
    ctxt->send_handler_cycles = 0;
    ctxt->recv_cycles = 0;
    ctxt->recv_handler_cycles = 0;
}
#endif

bool rts_is_on_list(PThreadContext *pCtxt, DeviceContext *dCtxt);

void rts_append(PThreadContext *pCtxt, DeviceContext *dCtxt);

extern "C" void softswitch_init(PThreadContext *ctxt)
{
    softswitch_softswitch_log(2, "softswitch_init : begin");

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    zeroProfileCounters(ctxt);
    #endif

    assert(!ctxt->pointersAreRelative);
    /*
  if(ctxt->pointersAreRelative){
    softswitch_softswitch_log(2, "softswitch_init : converting pointers from absolute to relative");
    for(unsigned i=0; i<ctxt->numDevices;i++){
      DeviceContext *dev=ctxt->devices+i;
      if(dev->properties!=0){
        dev->properties=softswitch_pthread_global_properties+(uintptr_t)dev->properties;
      }
      if(dev->state!=0){
        dev->state=softswitch_pthread_global_state+(uintptr_t)dev->state;
      }

      dev->vtable = (DeviceTypeVTable*)softswitch_device_vtables+(uintptr_t)dev->vtable;

      const DeviceTypeVTable *devVtable=dev->vtable;
      for(unsigned j=0; j<devVtable->numOutputs; j++){
        OutputPinTargets *target=(OutputPinTargets*)dev->targets+j;
        target->targets = (address_t*)( softswitch_pthread_global_properties + (uintptr_t)target->targets );
      }
      for(unsigned j=0; j<devVtable->numInputs; j++){
        InputPinSources *sources=(InputPinSources*)dev->sources+j;
        if(sources->sourceBindings){
          for(unsigned k=0; k<sources->numSources; k++){
            InputPinBinding *binding=(InputPinBinding*)sources->sourceBindings+k;
            if(binding->edgeProperties){
              binding->edgeProperties=softswitch_pthread_global_properties+(uintptr_t)binding->edgeProperties;
            }
          }
        }
      }
    }
    softswitch_softswitch_log(2, "softswitch_init : conversion done");
  }
  */



    ctxt->rtsHead=0;
    ctxt->rtsTail=0;

    for(unsigned i=0; i<ctxt->numDevices; i++){

        // Changed: now we do init here (previously we assumed it was done elsewhere

        DeviceContext *dev=ctxt->devices+i;
        const DeviceTypeVTable *vtable=dev->vtable;

        softswitch_softswitch_log(3, "softswitch_init : calling init handler for %s.", dev->id);

        // Needed for handler logging
        ctxt->currentDevice=i;
        ctxt->currentHandlerType=4; // Identify this as init handler
        //ctxt->currentPin=0;

        init_handler_t init = vtable->initHandler;

        init(ctxt->graphProps, dev->properties, dev->state);

        uint32_t flags = dev->vtable->readyToSendHandler(
            ctxt->graphProps,
            dev->properties,
            dev->state
        );
        dev->rtsFlags=flags;
        if(flags&0x7FFFFFFFul){
          rts_append(ctxt, dev);
        }
    }

    softswitch_softswitch_log(2, "softswitch_init : end.");
}
