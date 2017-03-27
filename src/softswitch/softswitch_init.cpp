#include "softswitch.hpp"

extern "C" int strcmp ( const char * str1, const char * str2 );

extern "C" void softswitch_init(PThreadContext *ctxt)
{   
    softswitch_softswitch_log(2, "softswitch_init : begin");
    
    ctxt->rtsHead=0;
    ctxt->rtsTail=0;
    
    for(unsigned i=0; i<ctxt->numDevices; i++){
        // Changed: now we do init here (previously we assumed it was done elsewhere
        
        DeviceContext *dev=ctxt->devices+i;
        DeviceTypeVTable *vtable=dev->vtable;
        
        softswitch_softswitch_log(4, "softswitch_init : looking for init handler for %s.", dev->id);
        for(unsigned pi=0; pi<vtable->numInputs; pi++){
            InputPortVTable *port=vtable->inputPorts+pi;
            if(!strcmp(port->name,"__init__")){
                softswitch_softswitch_log(3, "softswitch_init : calling init handler for %s.", dev->id);
                
                // Needed for handler logging
                ctxt->currentDevice=i;
                ctxt->currentHandlerType=1;
                ctxt->currentPort=pi;
                
                receive_handler_t handler=port->receiveHandler;

                handler(
                    ctxt->graphProps,
                    dev->properties,
                    dev->state,
                    nullptr, /*eProps*/
                    nullptr, /*eState*/
                    nullptr  /*message*/
                );
                
                ctxt->currentHandlerType=0;
                
                softswitch_softswitch_log(3, "softswitch_init : finished init handler for %s.", dev->id);
                break;
            }
        }
        
        softswitch_UpdateRTS(ctxt, dev);
    }
    
    softswitch_softswitch_log(3, "softswitch_init : end.");
}
