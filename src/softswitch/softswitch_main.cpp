#include "tinsel_api.hpp"
#include "hostMsg.hpp"

#include "softswitch.hpp"
//#include <cstdio>
#include <cstdarg>

#define HOSTBUFFER_SIZE 20 
#define MAX_HOST_PER_HANDLER 4

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

//! returns the size of the circular buffer (i.e. number of pending host messages)
uint8_t hostMsgBufferSize() {
  PThreadContext *ctxt=softswitch_getContext();
  uint8_t h = ctxt->hbuf_head;
  uint8_t t = ctxt->hbuf_tail;
  
  // need to deal with wraparound
  if(t < h) {
    return (t+HOSTBUFFER_SIZE) - h; 
  } else {
    return t-h;
  }
}

//! returns the space available in the circular buffer 
uint8_t hostMsgBufferSpace() { 
  return HOSTBUFFER_SIZE - hostMsgBufferSize();
}

//! Pushes a host message onto the circular buffer
void hostMsgBufferPush(hostMsg *msg) {
  PThreadContext *ctxt=softswitch_getContext();
  
  volatile hostMsg *buff = ctxt->hostBuffer;
  // stupidness to avoid memcpy TODO:fix
  buff[ctxt->hbuf_tail].type = msg->type; 
  buff[ctxt->hbuf_tail].id = msg->id;
  buff[ctxt->hbuf_tail].strAddr = msg->strAddr;
  for(unsigned i=0; i<HOST_MSG_PAYLOAD; i++) {
    buff[ctxt->hbuf_tail].parameters[i] = msg->parameters[i];
  }

  // increment the tail
  ctxt->hbuf_tail = ctxt->hbuf_tail + 1;
  if(ctxt->hbuf_tail == HOSTBUFFER_SIZE) {
    ctxt->hbuf_tail = 0; // wrap around
  } 
  
  return; 
}

//! pops and sends a message from the buffer
void hostMessageBufferPopSend(void *sendBuffer) {
  PThreadContext *ctxt=softswitch_getContext();
  
  // get the pointer to the hostMsgBuffer
  volatile hostMsg *buff = ctxt->hostBuffer;

  // get the address for the host
  int host = tinselHostId();

  // set the message length
  tinselSetLen(HOSTMSG_FLIT_SIZE);

  // get the slot for sending host messages
  volatile hostMsg* hmsg = (volatile hostMsg*)sendBuffer;
  hmsg->type = buff[ctxt->hbuf_head].type;
  hmsg->id = buff[ctxt->hbuf_head].id;
  hmsg->strAddr = buff[ctxt->hbuf_head].strAddr;
  for(unsigned i=0; i<HOST_MSG_PAYLOAD; i++) {
    hmsg->parameters[i] = buff[ctxt->hbuf_head].parameters[i];
  }

  // increment the head
  ctxt->hbuf_head = ctxt->hbuf_head + 1;
  if(ctxt->hbuf_head == HOSTBUFFER_SIZE) {
    ctxt->hbuf_head = 0; // wrap around
  }

  // Message prepped, sending
  tinsel_mboxSend(host, hmsg);

  // restore message size before returning
  tinsel_mboxSetLen(ctxt->currentSize);
     
  return;
}

extern "C" void softswitch_handler_log_impl(int level, const char *msg, ...)
{
  //Building a temporary handler log for playing about with this
  PThreadContext *ctxt=softswitch_getContext();
  assert(ctxt->currentHandlerType!=0); // can't call handler log when not in a handler
  assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

  if(level > ctxt->applLogLevel)
    return;

  const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;

  // create a hostMsg 
  hostMsg hmsg; 

  //prepare the message
  hmsg.id = tinselId();
  hmsg.type = 1; // magic number for STDOUT
  hmsg.strAddr = (unsigned)static_cast<const void*>(msg); 

  // Need to determine how many parameters there are
  unsigned param_cnt = 0;
  const char *in = msg;
  while(*in != '\0') {
    if(*in=='%'){
      param_cnt++; 
    } 
    in++;
  }
 
  // Temporary solution to check that we have enough space in the host message
  // just drops messages if they exceed the amount.
  // TODO:should be turned into an assertion once that support has been added.
  if(param_cnt > HOST_MSG_PAYLOAD)
    param_cnt = HOST_MSG_PAYLOAD; 
 
  // unpack the varadic parameters and place them in the message payload
  va_list args;
  va_start(args, msg); //msg : the named parameter preceeding the variadic
  for(unsigned i=0; i<param_cnt; ++i) {
        void* tmpvar = va_arg(args, void*);
        hmsg.parameters[i] = tmpvar; //type prune into bits for transfer
  }
  va_end(args);

  // push the message onto the hostMsgBuffer
  hostMsgBufferPush(&hmsg);

  return;
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
    
    // Create a software buffer for the host messages
    hostMsg hostMsgBuffer[HOSTBUFFER_SIZE];
    ctxt->hostBuffer = hostMsgBuffer;
    ctxt->hbuf_head = 0; // head of the host message buffer 
    ctxt->hbuf_tail = 0; // tail of the host message buffer 

    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSlot(0);


    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;
    uint32_t currSize=0;

    // Assumption: all buffers are owned by software, so we have to give them to mailbox
    // We only keep hold of slot 0 and [ tinsel_mboxSlotCount() - HOSTBUFFER_CNT, tinsel_mboxSlotCount()](for host comms)
    softswitch_softswitch_log(2, "Giving %d-%d receive buffers to mailbox", tinsel_mboxSlotCount(), HOSTBUFFER_SIZE);
    uint32_t max_non_host_message_slot = (tinsel_mboxSlotCount()-1);
    for(unsigned i=1; i<max_non_host_message_slot; i++){
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
             
             /* Either we have to finish sending a previous message to more
                addresses, or we get the chance to send a new message. 
                or we need to send a message to the host */
   
               if(!adequateHostBufferSpace) { // there are host messages that we need to flush first
                 softswitch_softswitch_log(3, "host buffer approaching full, prioritising hostbuff draining");
                 hostMessageBufferPopSend((void*)sendBuffer);                
               } else { // send a normal message
                   if(currSendTodo==0){
                       if(softswitch_IsRTSReady(ctxt)) {
                         softswitch_softswitch_log(3, "preparing new message");
    
                         // Prepare a new packet to send
                         currSize=softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList);
                         ctxt->currentSize = currSize; // update the current size in the thread context
                       } else if(!hostBufferEmpty) {
                         // sending a message to the host
                         softswitch_softswitch_log(3, "sending message to host");
                         hostMessageBufferPopSend((void*)sendBuffer);                
                       }    
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
            }
         
         
          softswitch_softswitch_log(3, "loop bottom");
      }
}
