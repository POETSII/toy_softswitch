#include "tinsel_api.hpp"
#include "hostMsg.hpp"

#include "softswitch.hpp"
//#include <cstdio>
#include <cstdarg>

#define HOSTBUFFER_SIZE 40 
#define MAX_HOST_PER_HANDLER 2 

#ifdef SOFTSWITCH_ENABLE_PROFILE
#include "softswitch_perfmon.hpp"
#else
typedef unsigned long size_t;
#endif

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

// extern definitions for host message buffer managment
// definitions are in softswitch_hostmessaging.cpp
extern "C" uint32_t hostMsgBufferSize(); // returns how many messages are in the buffer
extern "C" uint32_t hostMsgBufferSpace(); // returns remaining space is in the buffer (number of host messages)
extern "C" void hostMessageSlowPopSend(); // pops from the head of the buffer sends via UART (slow) TODO implement
extern "C" void hostMessageBufferPopSend(); // pops from the head of the buffer and sends via PCIe
//extern "C" void softswitch_handler_log_impl(int level, const char *msg, ...); // Adds a log message to the buffer
extern "C" template<typename ... Param> void softswitch_handler_log_impl(int level, const char *msg, const Param& ... param);


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
    
    // Create a software buffer for the host messages
    hostMsg hostMsgBuffer[HOSTBUFFER_SIZE];
    ctxt->hostBuffer = hostMsgBuffer;
    ctxt->hbuf_head = 0; // head of the host message buffer 
    ctxt->hbuf_tail = 0; // tail of the host message buffer 

    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSlot(0);
    // Slot 1 we will keep for host messages


    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;
    uint32_t currSize=0;

    // Assumption: all buffers are owned by software, so we have to give them to mailbox
    // We only keep hold of slot 0 and [ tinsel_mboxSlotCount() - HOSTBUFFER_CNT, tinsel_mboxSlotCount()](for host comms)
    softswitch_softswitch_log(2, "Giving %d receive buffers to mailbox", tinsel_mboxSlotCount()-2);
    for(unsigned i=2; i<tinsel_mboxSlotCount(); i++){
        tinsel_mboxAlloc( tinsel_mboxSlot(i) );
    }

    softswitch_softswitch_log(1, "starting loop");

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    ctxt->thread_cycles_tstart = tinsel_CycleCount();
    bool start_perfmon = false;
    unsigned perfmon_flush_rate_cnt = 1;
    const unsigned perfmon_flush_rate = SOFTSWITCH_ENABLE_PROFILE;
    #endif

    while(1) {
    
        softswitch_softswitch_log(2, "Loop top");
        
        // true if there is enough space in the host buffer to store the handlers
        // if this is false then draining the hostbuffer is a priority
        bool adequateHostBufferSpace = (hostMsgBufferSpace() >= MAX_HOST_PER_HANDLER);
        // true if there are NO items in the host buffer
        bool hostBufferEmpty = (hostMsgBufferSize() == 0);
        // Disallow host message to be dropped: when true host messages will be dropped when the host buffer is full
        bool canDropHostMessages = true; // true best effort where messages are dropped

        // Only want to send if either:
        // - we are currently sending message,
        // - or at least one device wants to send one
        // - or we need to send to the host
        bool wantToSend = (currSendTodo>0) || softswitch_IsRTSReady(ctxt) || !hostBufferEmpty;
        softswitch_softswitch_log(3, "wantToSend=%d", wantToSend?1:0);

        // Run idle if:
        // - There is nothing to receive
        // - we aren't able to send or we don't want to send
        #ifdef SOFTSWITCH_ENABLE_PROFILE
        uint32_t idle_start = tinsel_CycleCount();
        #endif
        while( (!tinsel_mboxCanRecv()) && (!wantToSend || !tinsel_mboxCanSend()) ){
            if(!softswitch_onIdle(ctxt)) {
                #ifdef SOFTSWITCH_ENABLE_PROFILE
                if(start_perfmon) {
                    ctxt->idle_cycles += deltaCycles(idle_start, tinsel_CycleCount()); 
                    if(perfmon_flush_rate_cnt < perfmon_flush_rate) {
                        perfmon_flush_rate_cnt = perfmon_flush_rate_cnt + 1;
                    } else {
                        ctxt->thread_cycles = deltaCycles(ctxt->thread_cycles_tstart, tinsel_CycleCount());
                        volatile uint32_t thread_start = tinsel_CycleCount();
                        ctxt->thread_cycles_tstart = thread_start;
                        softswitch_flush_perfmon();
                        ctxt->perfmon_cycles = deltaCycles(thread_start, tinsel_CycleCount());
                        perfmon_flush_rate_cnt = 1;
                    }
                }
                start_perfmon = true;
                #endif
                break;
	    }
        }

  
         uint32_t wakeupFlags = wantToSend ? (tinsel_CAN_RECV|tinsel_CAN_SEND) : tinsel_CAN_RECV;
         softswitch_softswitch_log(3, "waiting for send=%d, recv=%d", wakeupFlags&tinsel_CAN_SEND, (wakeupFlags&tinsel_CAN_RECV)?1:0);
  
         #ifdef SOFTSWITCH_ENABLE_PROFILE
         uint32_t blocked_start = tinsel_CycleCount(); 
         #endif
         tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );
         #ifdef SOFTSWITCH_ENABLE_PROFILE
         ctxt->blocked_cycles += deltaCycles(blocked_start, tinsel_CycleCount()); 
         #endif
         
         bool doRecv=false;
         bool doSend=false;

         // If true we cannot recevie messages as it might risk adding to the host buffer
         // we are blocked from receiving messages until the buffer has been cleared of space
         //bool hostBlock = (!canDropHostMessages && !adequateHostBufferSpace); 

         if(enableSendOverRecv){
             doSend=tinsel_mboxCanSend() && wantToSend;
             if(!doSend){
                 assert(tinsel_mboxCanRecv());
                 doRecv=true;
             }
         }else{
             // Default. Drain before fill.
  	     doRecv=tinsel_mboxCanRecv() && adequateHostBufferSpace;   
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
   
             // first clear any host messages
             if(!hostBufferEmpty) {           
               hostMessageBufferPopSend(); 
             } else {
  
             /* Either we have to finish sending a previous message to more
                addresses, or we get the chance to send a new message. 
                or we need to send a message to the host */
   
                   if(currSendTodo==0){
                       softswitch_softswitch_log(3, "preparing new message");
    
                       // Prepare a new packet to send
                       currSize=softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList);
                       ctxt->currentSize = currSize; // update the current size in the thread context
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
    
                       if(enableIntraThreadSend && currSendAddressList->thread==thisThreadId && adequateHostBufferSpace){
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
            }
         
         
          softswitch_softswitch_log(3, "loop bottom");
      }
}
