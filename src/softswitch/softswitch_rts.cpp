#include "softswitch.hpp"
#include "softswitch_hostmessaging.hpp"

#include <cstdio>

bool rts_is_on_list(PThreadContext *pCtxt, DeviceContext *dCtxt)
{
    DeviceContext *curr=pCtxt->rtsHead;
    while(curr){
        if(curr==dCtxt) return true;
        curr=curr->next;
    }
    return false;
}

static void rts_sanity(PThreadContext *pCtxt)
{
  #ifndef NDEBUG
    if(pCtxt->rtsHead != 0){
        assert(pCtxt->rtsTail!=0);
        
        DeviceContext *curr=pCtxt->rtsHead;
        while(curr){
            assert( (curr->rtsFlags&0x7FFFFFFFul)!=0);
            assert( (curr->prev == 0 && pCtxt->rtsHead==curr) || curr->prev->next==curr);
            assert( (curr->next == 0 && pCtxt->rtsTail==curr) || curr->next->prev==curr);
            curr=curr->next;
        }
    }else{
        assert(pCtxt->rtsHead==0);
    }
  #endif
}

// 5 instructions
void rts_append(PThreadContext *pCtxt, DeviceContext *dCtxt)
{
    rts_sanity(pCtxt);
    assert(!rts_is_on_list(pCtxt, dCtxt));
    
    dCtxt->prev=pCtxt->rtsTail; // 1
    dCtxt->next=0;       // 1
    if(pCtxt->rtsTail==0){     // 1
        pCtxt->rtsHead=dCtxt;   // 1
    }else{
        pCtxt->rtsTail->next=dCtxt; // 1
    }
    pCtxt->rtsTail=dCtxt; // 1
    
    rts_sanity(pCtxt);
}

// 8 instructions
static void rts_remove(PThreadContext *pCtxt, DeviceContext *dCtxt)
{
    rts_sanity(pCtxt);    
    assert(rts_is_on_list(pCtxt, dCtxt));
    
    softswitch_softswitch_log(4, "softswitch_UpdateRTS (%s) : softswitch_rts_remove:     begin, rtsHead=%p, rtsTail=%p, dCtxt->prev=%p, dCtxt->next=%p",
        dCtxt->id, pCtxt->rtsHead, pCtxt->rtsTail, dCtxt->prev, dCtxt->next
    );       
    
    assert(pCtxt->rtsHead);
    
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
    softswitch_softswitch_log(4, "softswitch_UpdateRTS (%s) : softswitch_rts_remove:     end", dCtxt->id);       
    
    rts_sanity(pCtxt);
}

// 6 instructions
static DeviceContext *rts_pop(PThreadContext *pCtxt)
{
    rts_sanity(pCtxt);
    
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

    rts_sanity(pCtxt);    
    
    return head;
}


void validate_rts(PThreadContext *ctxt)
{
    for(unsigned i=0; i<ctxt->numDevices; i++){
        auto ddev=ctxt->devices+i;
        ctxt->currentHandlerType=10;
        ctxt->currentDevice=ddev->index;
        uint32_t rtsG=ddev->vtable->readyToSendHandler(ctxt->graphProps, ddev->properties, ddev->state);
        assert(rtsG==ddev->rtsFlags);
        assert( ((dev->rtsFlags&0x7FFFFFFFul)!=0) == rts_is_on_list(ctxt, dev));
    }
}

extern "C" void softswitch_UpdateRTS(
    PThreadContext *pCtxt, DeviceContext *dev
){
    rts_sanity(pCtxt);
    
    // Find out if the handler wants to send or compute in the future
    softswitch_softswitch_log(3, "softswitch_UpdateRTS (%s) : begin", dev->id);    
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

    assert(anyReadyPrev==rts_is_on_list(pCtxt, dev));

    ((volatile DeviceContext*)dev)->rtsFlags=flags; 
    
    // Check if overall output RTS status is the same (ignoring which pins) 
    if( anyReadyPrev == anyReadyNow ){
        softswitch_softswitch_log(4, "softswitch_UpdateRTS (%s) : done (no change), prev=%x, now=%x", dev->id, dev->rtsFlags, flags);       
        return;
    }
    
    if(anyReadyNow){
        softswitch_softswitch_log(4, "softswitch_UpdateRTS (%s) : adding to RTS list", dev->id);       

        rts_append(pCtxt, dev);
    }else{
        softswitch_softswitch_log(3, "softswitch_UpdateRTS (%s) : removing from RTS list", dev->id);       
        rts_remove(pCtxt, dev);
    }
    
    softswitch_softswitch_log(3, "softswitch_UpdateRTS (%s) : done, flags=%x", dev->id, dev->rtsFlags);       
}

extern "C" DeviceContext *softswitch_PopRTS(PThreadContext *pCtxt)
{
    auto dev=rts_pop(pCtxt);
   // // assert(dev);
    // These need to be recomputed
    return dev;
}

extern "C" bool softswitch_IsRTSReady(PThreadContext *pCtxt)
{
    return pCtxt->rtsHead!=0;
}
