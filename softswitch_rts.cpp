#include "softswitch.hpp"

// 5 instructions
static void rts_append(PThreadContext *pCtxt, DeviceContext *dCtxt)
{
    dCtxt->prev=pCtxt->rtsTail; // 1
    dCtxt->next=0;       // 1
    if(pCtxt->rtsTail==0){     // 1
        pCtxt->rtsHead=dCtxt;   // 1
    }else{
        pCtxt->rtsTail->next=dCtxt; // 1
    }
    pCtxt->rtsTail=dCtxt; // 1
}

// 8 instructions
static void rts_remove(PThreadContext *pCtxt, DeviceContext *dCtxt)
{
    if(pCtxt->rtsHead==dCtxt){          // 1
        pCtxt->rtsHead=dCtxt->next;     // 1
    }else{
        dCtxt->prev->next=dCtxt->next; // 3
    }
    if(pCtxt->rtsTail==dCtxt){          // 1
        pCtxt->rtsTail=dCtxt->prev;     // 1
    }else{
        dCtxt->next->prev=dCtxt->prev; // 3
    }
}

// 6 instructions
static DeviceContext *rts_pop(PThreadContext *pCtxt)
{
    auto head=pCtxt->rtsHead;   // 1
    if(head){            // 1
        assert(head->prev==0);
        
        auto next=head->next; // 1
        if(next){             // 1
            next->prev=0;     // 1
        }else{ 
            pCtxt->rtsTail=0;        // 1
        }
        pCtxt->rtsHead=next;         // 1
    }
    return head;
}




extern "C" void softswitch_UpdateRTS(
    PThreadContext *pCtxt, DeviceContext *dev
){
    // Find out if the handler wants to send or compute in the future
    uint32_t flags = dev->vtable->readyToSendHandler(
        pCtxt->graphProps,
        dev->properties,
        dev->state
    );
    
    // Look at RTC flag
    auto rtc = flags&0x80000000ul;
    if( rtc ){
        pCtxt->rtcChecked=0; // Force another complete loop
    }
    
    // Flags with compute stripped off - are 
    bool anyReadyPrev = 0 != (dev->rtsFlags&0x7FFFFFFFul);
    bool anyReadyNow = 0 != (flags&0x7FFFFFFFul);
    
    // Check if overall output RTS status is the same (ignoring which ports) 
    if( anyReadyPrev == anyReadyNow ){
        return;
    }
    
    if(anyReadyNow){
        rts_append(pCtxt, dev);
    }else{
        rts_remove(pCtxt, dev);
    }
    dev->rtsFlags=flags; 
}

extern "C" DeviceContext *P_PopRTS(PThreadContext *pCtxt)
{
    return rts_pop(pCtxt);
}

extern "C" bool P_IsRTSReady(PThreadContext *pCtxt)
{
    return pCtxt->rtsHead!=0;
}
