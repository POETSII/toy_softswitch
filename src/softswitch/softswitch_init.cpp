#include "softswitch.hpp"

extern "C" void softswitch_init(PThreadContext *ctxt)
{   
    ctxt->rtsHead=0;
    ctxt->rtsTail=0;
    
    for(unsigned i=0; i<ctxt->numDevices; i++){
        // Assume that __init__ was handled by orchestrator/loader if nesc.
        
        softswitch_UpdateRTS(ctxt, ctxt->devices+i);
    }
}
