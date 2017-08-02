#include "tinsel_api.hpp"

#include "softswitch.hpp"
//#include <cstdio>
#include <cstdarg>

typedef unsigned long size_t;

//! Initialise data-structures (e.g. RTS)
extern "C" void softswitch_init(PThreadContext *ctxt);

//! Do some idle work. Return false if there is nothing more to do
extern "C" bool softswitch_onIdle(PThreadContext *ctxt);

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message);

//! Prepare a message on the given buffer
/*! \param numTargets Receives the number of addresses to send the packet to
    \param pTargets If numTargets>0, this will be pointed at an array with numTargets elements
    \retval Size of message in bytes
*/
extern "C" unsigned softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets);

extern "C" int vsnprintf (char * s, size_t n, const char * format, va_list arg );

extern "C" void *memset( void *dest, int ch, size_t count );

static PThreadContext *softswitch_getContext()
{
  if(tinsel_myId() < softswitch_pthread_count){
    return softswitch_pthread_contexts + tinsel_myId();
  }else{
    return 0;
  }
}

#ifndef POETS_DISABLE_LOGGING


static void append_vprintf(int &left, char *&dst, const char *msg, va_list v)
{
    int done=vsnprintf(dst, left, msg, v);
    if(done>=left){
      done=left-1;
    }
    dst+=done;
    left-=done;
}

static void append_printf(int &left, char *&dst, const char *msg, ...)
{
    va_list v;
    va_start(v,msg);
    append_vprintf(left, dst, msg, v);
    va_end(v);
}


extern "C" void softswitch_softswitch_log_impl(int level, const char *msg, ...)
{
    PThreadContext *ctxt=softswitch_getContext();
    
    if(level > ctxt->softLogLevel)
        return;
    
    char buffer[256];
    int left=sizeof(buffer)-3;
    char *dst=buffer;

    for(int i=0; i<255; i++) { buffer[i] = 0; } //To-Do replace with something better

    append_printf(left, dst, "[%08x] SOFT : ", tinsel_myId());
    
    va_list v;
    va_start(v,msg);
    append_vprintf(left, dst, msg, v);
    va_end(v);
    
    append_printf(left, dst, "\n");

    tinsel_puts(buffer);
}

extern "C" void softswitch_handler_log_impl(int level, const char *msg, ...)
{
    PThreadContext *ctxt=softswitch_getContext();
    assert(ctxt->currentHandlerType!=0); // can't call handler log when not in a handler
    assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

    if(level > ctxt->applLogLevel)
        return;

    char buffer[256];
    int left=sizeof(buffer)-3;
    char *dst=buffer;
    
    for(int i=0; i<255; i++) { buffer[i] = 0; } //To-Do replace with something better

    const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;
    
    append_printf(left, dst, "[%08x] APPL / device (%u)=%s", tinsel_myId(), ctxt->currentDevice, deviceContext->id); 

    if(ctxt->currentHandlerType==1){
        auto pin=deviceContext->vtable->inputPins[ctxt->currentPin].name;
        append_printf(left, dst, " / recv / %s : ", pin);
    }else if(ctxt->currentHandlerType==2){
        auto pin=deviceContext->vtable->outputPins[ctxt->currentPin].name;
        append_printf(left, dst, " / send / %s : ", pin);
    }else if(ctxt->currentHandlerType==3){
        append_printf(left, dst, " / comp / compute : ");
    }

    va_list v;
    va_start(v,msg);
    append_vprintf(left, dst, msg, v);
    va_end(v);
    
    append_printf(left, dst, "\n");

    tinsel_puts(buffer);
}

#endif

#ifdef SOFTSWITCH_ENABLE_INTRA_THREAD_SEND
const int enableIntraThreadSend=1;
#else
const int enableIntraThreadSend=0;
#endif

#ifdef SOFTSWITCH_ENABLE_SEND_OVER_RECV
const int enableSendOverRecv=1;
#else
const int enableSendOverRecv=0;
#endif



extern "C" void softswitch_main()
{ 
    PThreadContext *ctxt=softswitch_getContext();
    if(ctxt==0){
      //tinsel_puts("softswitch_main - no thread\n");
      while(1);
    }

    unsigned thisThreadId=tinsel_myId();
    
    
    softswitch_softswitch_log(1, "softswitch_main()");
    softswitch_init(ctxt);
    
    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSlot(0);

    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;
    uint32_t currSize=0;

    // Assumption: all buffers are owned by software, so we have to give them to mailbox
    // We only keep hold of slot 0
    softswitch_softswitch_log(2, "Giving %d-1 receive buffers to mailbox", tinsel_mboxSlotCount());
    for(unsigned i=1; i<tinsel_mboxSlotCount(); i++){
        tinsel_mboxAlloc( tinsel_mboxSlot(i) );
    }

    softswitch_softswitch_log(1, "starting loop");
    
    while(1) {
        softswitch_softswitch_log(2, "Loop top");
        
        // Only want to send if either:
        // - we are currently sending message,
        // - or at least one device wants to send one
        bool wantToSend = (currSendTodo>0) || softswitch_IsRTSReady(ctxt);
        softswitch_softswitch_log(3, "wantToSend=%d", wantToSend?1:0);


        // Run idle if:
        // - There is nothing to receive
        // - we aren't able to send or we don't want to send
        while( (!tinsel_mboxCanRecv()) && (!wantToSend || !tinsel_mboxCanSend()) ){
            if(!softswitch_onIdle(ctxt))
                break;
        }

        uint32_t wakeupFlags = wantToSend ? (tinsel_CAN_RECV|tinsel_CAN_SEND) : tinsel_CAN_RECV;
        softswitch_softswitch_log(3, "waiting for send=%d, recv=%d", wakeupFlags&tinsel_CAN_SEND, (wakeupFlags&tinsel_CAN_RECV)?1:0);
        tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );

        bool doRecv=false;
        bool doSend=false;

        if(enableSendOverRecv){
            doSend=tinsel_mboxCanSend() && wantToSend;
            if(!doSend){
                assert(tinsel_mboxCanRecv());
                doRecv=true;
            }
        }else{
            // Default. Drain before fill.
	  doRecv=tinsel_mboxCanRecv();
	  if(!doRecv){
	    assert(tinsel_mboxCanSend() && wantToSend);
	    doSend=true;
	  }
        }

        if(doRecv){
            softswitch_softswitch_log(2, "receive branch");
            assert(tinsel_mboxCanRecv());

            /*! We always receive messages, even while a send is in progress (currSendTodo > 0) */

            softswitch_softswitch_log(4, "calling mboxRecv");
            auto recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool
	    assert(recvBuffer);

            softswitch_onReceive(ctxt, (const void *)recvBuffer);  // Decode and dispatch

            softswitch_softswitch_log(4, "giving buffer back");
            tinsel_mboxAlloc(recvBuffer); // Put it back in receive pool
        }
        if(doSend){
            softswitch_softswitch_log(2, "send branch");

            assert(wantToSend); // Only come here if we have something to do
            assert(tinsel_mboxCanSend()); // Only reason we could have got here

            /* Either we have to finish sending a previous message to more
               addresses, or we get the chance to send a new message. */

            if(currSendTodo==0){
                softswitch_softswitch_log(3, "preparing new message");

                // Prepare a new packet to send
                currSize=softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList);
            }else{
                // We still have more addresses to deliver the last message to
                softswitch_softswitch_log(3, "forwarding current message");
            }
            
            if(currSendTodo>0){
                assert(currSendAddressList);
                
                softswitch_softswitch_log(3, "sending to thread %08x, device %u, pin %u", currSendAddressList->thread, currSendAddressList->device, currSendAddressList->pin);
                
                // Update the target address (including the device and pin)
		static_assert(sizeof(address_t)==8);
		// This wierdness is to avoid the compiler turning it into a call to memcpy
                *(uint64_t*)&((packet_t*)sendBuffer)->dest = *(uint64_t*)currSendAddressList;

                if(enableIntraThreadSend && currSendAddressList->thread==thisThreadId){
                    // Deliver on this thread without waiting
                    softswitch_onReceive(ctxt, (const void *)sendBuffer);  // Decode and dispatch
                }else{
                    // Send to the relevant thread
                    // TODO: Shouldn't there be something like mboxForward as part of
                    // the API, which only takes the address?
                    softswitch_softswitch_log(4, "setting length to %u", currSize);
                    tinsel_mboxSetLen(currSize);
                    softswitch_softswitch_log(4, "doing send");
                    tinsel_mboxSend(currSendAddressList->thread, sendBuffer);
                }

                // Move onto next address for next time
                currSendTodo--; // If this reaches zero, we are done with the message
                currSendAddressList++;
            }
        }
        
        softswitch_softswitch_log(3, "loop bottom");
    }
}
