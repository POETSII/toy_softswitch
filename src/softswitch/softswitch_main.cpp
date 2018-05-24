#include "tinsel_api.hpp"
#include "hostMsg.hpp"

#include "softswitch.hpp"
#include "softswitch_hostmessaging.hpp"
//#include <cstdio>
#include <cstdarg>

//#define SINGLESTEPPING

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
extern "C" unsigned softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets, uint8_t *isApp);

extern "C" void *memset( void *dest, int ch, size_t count );

//! functions used to manage the hostmessaging buffer
extern "C" uint32_t hostMsgBufferSize();
extern "C" uint32_t hostMsgBufferSpace();
extern "C" void hostMsgBufferPush(hostMsg *msg);

static PThreadContext *softswitch_getContext()
{
  if(tinsel_myId() < softswitch_pthread_count){
    return softswitch_pthread_contexts + tinsel_myId();
  }else{
    return 0;
  }
}

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
      while(1);
    }

    unsigned thisThreadId=tinsel_myId();

    #ifdef SINGLESTEPPING // if we are in sequential execution mode then thread 1 kicks it off
      bool token;
      if(thisThreadId == 0) { 
        token = true; // start with the token of execution
      } else {
        token = false;
      }
    #endif
    
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
    uint8_t isApp=0; // is 1 if we are sending on an application pin

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
                        // check if there is space in the host buffer
                        if(!hostMsgBufferSpace()) { // if not flush an element of the buffer over UART
                          hostMessageSlowPopSend(); // clear space
                        }
                        softswitch_flush_perfmon(); // add the perfmon data to host message buffer
                        ctxt->perfmon_cycles = deltaCycles(thread_start, tinsel_CycleCount());
                        perfmon_flush_rate_cnt = 1;
                    }
                }
                start_perfmon = true;
                #endif
                break;
	    }
        }

         #ifdef SINGLESTEPPING
         // we can only procced if we have the execution token 
         volatile void* outstanding_recvs[16];
         uint32_t num_outstanding_recvs=0;         
         softswitch_softswitch_log(2, "waiting for token");
         while(!token) {
             if(tinsel_mboxCanRecv()) {
                volatile void* recvBuffer=tinsel_mboxRecv(); // Take the buffer from receive pool
  	        assert(recvBuffer);
                volatile const packet_t *curItem = (volatile const packet_t *)recvBuffer;
                if(curItem->dest.pin != 0xFF) { // This is not a token pin message
                  softswitch_softswitch_log(2, "Adding recv message to the outstanding recv buffer");
                  outstanding_recvs[num_outstanding_recvs] = recvBuffer;
                  num_outstanding_recvs++; 
                } else {
                  token = true; 
                  tinsel_mboxAlloc(recvBuffer); // Put it back in receive pool
                }
             }
        }
         softswitch_softswitch_log(2, "got the token");
         #endif // SINGLESTEPPING

         #ifdef SINGLESTEPPING
            uint32_t wakeupFlags = (tinsel_CAN_RECV | tinsel_CAN_SEND); 
         #else 
             uint32_t wakeupFlags = wantToSend ? (tinsel_CAN_RECV|tinsel_CAN_SEND) : tinsel_CAN_RECV;
             softswitch_softswitch_log(3, "waiting for send=%d, recv=%d", wakeupFlags&tinsel_CAN_SEND, (wakeupFlags&tinsel_CAN_RECV)?1:0);
         #endif
  
  
         #ifdef SOFTSWITCH_ENABLE_PROFILE
         uint32_t blocked_start = tinsel_CycleCount(); 
         #endif
         tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );
         #ifdef SOFTSWITCH_ENABLE_PROFILE
         if(wantToSend) {
             ctxt->send_blocked_cycles += deltaCycles(blocked_start, tinsel_CycleCount()); 
         } else {
             ctxt->recv_blocked_cycles += deltaCycles(blocked_start, tinsel_CycleCount()); 
         }
         #endif
         
         bool doRecv=false;
         bool doSend=false;

         while(!adequateHostBufferSpace) { //Drain the host buffer over UART to prevent deadlock
           hostMessageSlowPopSend(); 
           adequateHostBufferSpace = (hostMsgBufferSpace() >= MAX_HOST_PER_HANDLER);
         }

         if(enableSendOverRecv){
             doSend=tinsel_mboxCanSend() && wantToSend;
             if(!doSend){
                 #ifdef SINGLESTEPPING
                   assert(tinsel_mboxCanRecv() || token);
                   if(tinsel_mboxCanRecv()) 
                       doRecv=true;
                 #else
                   assert(tinsel_mboxCanRecv());
                   doRecv=true;
                 #endif // SINGLESTEPPING
             }
         }else{
             // Default. Drain before fill.
  	     doRecv=tinsel_mboxCanRecv() && adequateHostBufferSpace;   
  	     if(!doRecv){
               #ifdef SINGLESTEPPING
  	         assert((tinsel_mboxCanSend() && wantToSend) || token);
                 if(wantToSend)
                    doSend=true;
               #else
  	         assert(tinsel_mboxCanSend() && wantToSend);
  	         doSend=true;
               #endif
  	     }
         }
  
         if(doRecv){
             softswitch_softswitch_log(2, "receive branch");
 
             #ifdef SINGLESTEPPING
               assert(tinsel_mboxCanRecv() | token);
             #else
               assert(tinsel_mboxCanRecv());
             #endif
  
             /*! We always receive messages, even while a send is in progress (currSendTodo > 0) */
  
             volatile void* recvBuffer;
             #ifdef SINGLESTEPPING
               if(num_outstanding_recvs > 0) {
                 softswitch_softswitch_log(2, "processing message from the outstanding recv buffer");
                 recvBuffer = outstanding_recvs[num_outstanding_recvs];
                 num_outstanding_recvs--;
                 assert(recvBuffer);
               } else {
                 softswitch_softswitch_log(2, "calling mboxRecv");
                 recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool
  	         assert(recvBuffer);
               }
             #else
               softswitch_softswitch_log(4, "calling mboxRecv");
               recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool
  	       assert(recvBuffer);
             #endif

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
                       currSize=softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList, &isApp);
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
                       if(isApp) { // we are sending an application message there is no destination
                          //*(uint64_t*)&((packet_t*)sendBuffer)->dest = tinselHostId(); 
                       } else { // we are sending a regular message
    	                 // This wierdness is to avoid the compiler turning it into a call to memcpy
                         *(uint64_t*)&((packet_t*)sendBuffer)->dest = *(uint64_t*)currSendAddressList;
                       }
    
                       if(enableIntraThreadSend && currSendAddressList->thread==thisThreadId && adequateHostBufferSpace){
                           // Deliver on this thread without waiting
                           softswitch_onReceive(ctxt, (const void *)sendBuffer);  // Decode and dispatch
                       }else{
                           // Send to the relevant thread
                           // TODO: Shouldn't there be something like mboxForward as part of
                           // the API, which only takes the address?
                           softswitch_softswitch_log(3, "setting length to %u", currSize);
                           tinsel_mboxSetLen(currSize);
                           if(isApp) { // an application message
                             softswitch_softswitch_log(3, "doing application send");
                             tinsel_mboxSetLen(64);
                             tinsel_mboxSend(tinselHostId(), sendBuffer);
                             currSendTodo = 0;
                           } else { // not an application message
                             softswitch_softswitch_log(3, "doing send");
                             tinsel_mboxSend(currSendAddressList->thread, sendBuffer);
                           }
                       }
    
                       if(!isApp) { // we are only sending to multiple addresses if it is not an application message
                         // Move onto next address for next time
                         currSendTodo--; // If this reaches zero, we are done with the message
                         currSendAddressList++;
                       }
               }
              }
            } //doSend

          
          #ifdef SINGLESTEPPING
             
            // pass the token along to the next thread
            assert(token); // ensure that we have the token of execution 
            //TODO: make this configurable so it's not limited to single board systems
            uint32_t nextThread;
            //if(thisThreadId < (1024-1))
            if(thisThreadId < (10-1))
              nextThread = thisThreadId+1; 
            else
              nextThread = 0;
            
            softswitch_softswitch_log(3, "passing the token onto %x", nextThread);

            // wait until we can send
            tinselWaitUntil(TINSEL_CAN_SEND);

            // send the token on
            tinselSetLen(3); // max size 
            packet_t* tokenMsg = (packet_t*)sendBuffer;
            tokenMsg->dest.thread = nextThread;
            tokenMsg->dest.device = 0;
            tokenMsg->dest.pin = 0xFF; 
            tokenMsg->size = sizeof(packet_t);
            tinsel_mboxSend(nextThread, sendBuffer);

            token = false; // token has been passed on 
            // wait until we can send
            tinselWaitUntil(TINSEL_CAN_SEND);
          #endif // SINGLESTEPPING

          softswitch_softswitch_log(3, "loop bottom");
      }
}
