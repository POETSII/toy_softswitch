#include "tinsel_api.hpp"
#include "hostMsg.hpp"

#include "softswitch.hpp"
#include "softswitch_hostmessaging.hpp"
//#include <cstdio>
#include <cstdarg>

#ifdef SOFTSWITCH_ENABLE_PROFILE
#include "softswitch_perfmon.hpp"
#else
// typedef unsigned long size_t;
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

//! Complete a hardware step
extern "C" void softswitch_on_hardware_idle(PThreadContext *ctxt);

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
    if(ctxt==0 || ctxt->numDevices==0){
      while(1){
        tinsel_mboxIdle(false);
      }
    }

    unsigned thisThreadId=tinsel_myId();

    // Create a software buffer for the host messages
    hostMsg hostMsgBuffer[HOSTBUFFER_SIZE];
    ctxt->hostBuffer = hostMsgBuffer;
    ctxt->hbuf_head = 0; // head of the host message buffer
    ctxt->hbuf_tail = 0; // tail of the host message buffer

    softswitch_softswitch_log(1, "softswitch_main()");
    softswitch_init(ctxt);

    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSendSlot();


    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;
    uint32_t currSize=0;
    uint8_t isApp=0; // is 1 if we are sending on an application pin

    softswitch_softswitch_log(2, "start loop");

    #ifdef SOFTSWITCH_ENABLE_PROFILE
    ctxt->thread_cycles_tstart = tinsel_CycleCount();
    bool start_perfmon = false;
    unsigned perfmon_flush_rate_cnt = 1;
    const unsigned perfmon_flush_rate = SOFTSWITCH_ENABLE_PROFILE;
    #endif

    while(1) {

        softswitch_softswitch_log(3, "Loop top");

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
        if( (!tinsel_mboxCanRecv()) && (!wantToSend || !tinsel_mboxCanSend()) ){
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
	        }else{
            continue;
          }
        }

        uint32_t wakeupFlags = wantToSend ? (tinsel_CAN_RECV|tinsel_CAN_SEND) : tinsel_CAN_RECV;
        softswitch_softswitch_log(4, "waiting for send=%d, recv=%d", wakeupFlags&tinsel_CAN_SEND, (wakeupFlags&tinsel_CAN_RECV)?1:0);

         #ifdef SOFTSWITCH_ENABLE_PROFILE
         uint32_t blocked_start = tinsel_CycleCount();
         #endif
         #if SOFTSWITCH_ENABLE_HARDWARE_IDLE
         bool doHardwareIdle=false;
         if(!wantToSend){
           for(unsigned i=0; i<ctxt->numDevices; i++){
             assert(ctxt->devices[i].rtsFlags==0);
           }
            bool vote=false;
            doHardwareIdle=tinsel_mboxIdle(vote);
         }else{
            tinsel_mboxWaitUntil( (tinsel_WakeupCond) (tinsel_CAN_RECV|tinsel_CAN_SEND) );
         }
         #else
         tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );
         #endif
         #ifdef SOFTSWITCH_ENABLE_PROFILE
         if(wantToSend) {
             ctxt->send_blocked_cycles += deltaCycles(blocked_start, tinsel_CycleCount());
         } else {
             ctxt->recv_blocked_cycles += deltaCycles(blocked_start, tinsel_CycleCount());
         }
         #endif

         #if SOFTSWITCH_ENABLE_HARDWARE_IDLE
          if(doHardwareIdle){
            softswitch_on_hardware_idle(ctxt);
            continue;
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

             volatile void* recvBuffer;

              softswitch_softswitch_log(4, "calling mboxRecv");
              recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool
  	          assert(recvBuffer);

             softswitch_onReceive(ctxt, (const void *)recvBuffer);  // Decode and dispatch

             softswitch_softswitch_log(4, "giving buffer back");
             tinsel_mboxFree(recvBuffer); // Put it back in receive pool

         }
         if(doSend){
             softswitch_softswitch_log(3, "send branch");

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
                       //assert(currSendAddressList);

                       softswitch_softswitch_log(3, "sending to thread %x, device %u, pin %u", currSendAddressList->thread, currSendAddressList->device, currSendAddressList->pin);

                       // Update the target address (including the device and pin)
    	               static_assert(sizeof(address_t)==8);
                       if(isApp) { // we are sending an application message there is no destination
                          //*(uint64_t*)&((packet_header_t*)sendBuffer)->dest = tinselHostId();
                       } else { // we are sending a regular message
    	                 // This wierdness is to avoid the compiler turning it into a call to memcpy
                         *(uint64_t*)&((packet_header_t*)sendBuffer)->dest = *(uint64_t*)currSendAddressList;
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

          softswitch_softswitch_log(3, "loop bottom");
      }
}
