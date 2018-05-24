#include "softswitch_hostmessaging.hpp"

//! returns the size of the buffer (i.e. number of pending host messages)
uint32_t hostMsgBufferSize() {
  PThreadContext *ctxt=softswitch_getCtxt();
  uint32_t h = ctxt->hbuf_head;
  uint32_t t = ctxt->hbuf_tail;

  // need to deal with wraparound
  if(t < h) {
    return (t+HOSTBUFFER_SIZE) - h;
  } else {
    return t-h;
  }
}

//! returns the space available in the buffer 
uint32_t hostMsgBufferSpace() {
  return HOSTBUFFER_SIZE - hostMsgBufferSize();
}

//! Pushes a host message onto the buffer
void hostMsgBufferPush(hostMsg *msg) {
  PThreadContext *ctxt=softswitch_getCtxt();

  volatile hostMsg *buff = ctxt->hostBuffer;
  // stupidness to avoid memcpy TODO:fix
  buff[ctxt->hbuf_tail].type = msg->type;
  buff[ctxt->hbuf_tail].source.thread = msg->source.thread;
  for(unsigned i=0; i<HOST_MSG_PAYLOAD; i++) {
    buff[ctxt->hbuf_tail].payload[i] = msg->payload[i];
  }

  // increment the tail
  ctxt->hbuf_tail = ctxt->hbuf_tail + 1;
  if(ctxt->hbuf_tail == HOSTBUFFER_SIZE) {
    ctxt->hbuf_tail = 0; // wrap around
  }

  return;
}

//! pops and sends a message from the buffer
void hostMessageBufferPopSend() {
  PThreadContext *ctxt=softswitch_getCtxt();

  // get the pointer to the hostMsgBuffer
  volatile hostMsg *buff = ctxt->hostBuffer;

  // get the address for the host
  int host = tinselHostId();

  // set the message length
  tinsel_mboxSetLen(sizeof(hostMsg));
  //tinselSetLen(HOSTMSG_FLIT_SIZE);

  // get the slot for sending host messages
  volatile hostMsg* hmsg = (volatile hostMsg*)tinsel_mboxSlot(1);
  hmsg->type = buff[ctxt->hbuf_head].type;
  hmsg->source.thread = buff[ctxt->hbuf_head].source.thread;
  for(unsigned i=0; i<HOST_MSG_PAYLOAD; i++) {
    hmsg->payload[i] = buff[ctxt->hbuf_head].payload[i];
  }

  // increment the head
  ctxt->hbuf_head = ctxt->hbuf_head + 1;
  if(ctxt->hbuf_head == HOSTBUFFER_SIZE) {
    ctxt->hbuf_head = 0; // wrap around
  }

  // Message prepped, sending
  tinsel_mboxSend(host, hmsg);

  return;
}

/* Wrapper around the tinselUartTryPut() function, keeps trying until successful. 
*/
void tinsel_UARTPut(uint8_t x) {
  while(!tinselUartTryPut(x)) { }
  return;
}

//! slow hostlink - sends data up the hostlink instead of via PCIe messages
// pops the data off the buffer and passes it up the UART, can be used in situations where the buffer is full
// to avoid deadlocking the system
// This should (hopefully) be rarely called, so I (sf306) believe that it is reasonable to assume that the
// hostmessage is taking the maximum size of 4 flits.
void hostMessageSlowPopSend() {
  // Protocol for sending host messages over this channel
  // 16-bits of ID (hi) and 8-bits of payload (lo)

  // setup the context and prefix id for sending down UART
  uint32_t prefix=tinselId()<<8; 
  PThreadContext *ctxt=softswitch_getCtxt();
  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
 
  // get the host buffer
  volatile hostMsg *buff = ctxt->hostBuffer;
  
  // get the hostMsg at the head
  volatile hostMsg *hmsg = &buff[ctxt->hbuf_head];
  
  // sending the message type 
  tinsel_UARTPut(prefix | ((hmsg->type>>0)&0xFF) ); //LSBs
  tinsel_UARTPut(prefix | ((hmsg->type>>8)&0xFF) ); //MSBs

  // sending each element of the hostMsg payload
  for(uint32_t i=0; i<HOST_MSG_PAYLOAD; i++) {
    tinsel_UARTPut(prefix | ((hmsg->payload[i]>>0)&0xFF)); //LSBs
    tinsel_UARTPut(prefix | ((hmsg->payload[i]>>8)&0xFF)); 
    tinsel_UARTPut(prefix | ((hmsg->payload[i]>>16)&0xFF)); 
    tinsel_UARTPut(prefix | ((hmsg->payload[i]>>24)&0xFF)); //MSBs
  }
  
  // increment the buffer head to pop the message
  ctxt->hbuf_head = ctxt->hbuf_head + 1;
  if(ctxt->hbuf_head == HOSTBUFFER_SIZE) {
    ctxt->hbuf_head = 0; // wrap around
  }

  return;
  
}

//! directHostMessageSlowSend
// bypasses the buffer and the main softswitch event loop and sends the hostmessage directly
// this is used to send asserts etc.. where the rest of the softswitch should stop executing.
void directHostMessageSlowSend(hostMsg *msg) {

  // setup the context and prefix id for sending down UART
  uint32_t prefix=tinselId()<<8; 
  PThreadContext *ctxt=softswitch_getCtxt();
  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
 
  // sending the message type 
  tinsel_UARTPut(prefix | ((msg->type>>0)&0xFF) ); //LSBs
  tinsel_UARTPut(prefix | ((msg->type>>8)&0xFF) ); //MSBs

  // sending each element of the hostMsg payload
  for(uint32_t i=0; i<HOST_MSG_PAYLOAD; i++) {
    tinsel_UARTPut(prefix | ((msg->payload[i]>>0)&0xFF)); //LSBs
    tinsel_UARTPut(prefix | ((msg->payload[i]>>8)&0xFF)); 
    tinsel_UARTPut(prefix | ((msg->payload[i]>>16)&0xFF)); 
    tinsel_UARTPut(prefix | ((msg->payload[i]>>24)&0xFF)); //MSBs
  }
  
  return;
}

/*
   Perfmon calls
*/
#ifdef SOFTSWITCH_ENABLE_PROFILE

//! softswitch_flush_perfmon 
// flushes the performance monitoring counters  
extern "C" void softswitch_flush_perfmon() {

  static_assert(HOST_MSG_PAYLOAD >= 8, "Not enough space in the host message struct to flush all perfmon counters");

  PThreadContext *ctxt=softswitch_getCtxt();
  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;

  hostMsg msg;
  msg.source.thread = tinselId(); // Id of this thread
  msg.type = 0xE0; // magic number for perfmon dump  

  // load the performance counter values into the payload
  msg.payload[0] = ctxt->thread_cycles;
  msg.payload[1] = ctxt->send_blocked_cycles;
  msg.payload[2] = ctxt->recv_blocked_cycles;
  msg.payload[3] = ctxt->idle_cycles;
  msg.payload[4] = ctxt->perfmon_cycles;
  msg.payload[5] = ctxt->send_cycles;
  msg.payload[6] = ctxt->send_handler_cycles;
  msg.payload[7] = ctxt->recv_cycles;
  msg.payload[8] = ctxt->recv_handler_cycles;

  // push onto the back of the buffer
  hostMsgBufferPush(&msg);

  // reset all the performance counters
  ctxt->thread_cycles = 0;
  ctxt->send_blocked_cycles = 0;
  ctxt->recv_blocked_cycles = 0;
  ctxt->idle_cycles = 0;
  ctxt->perfmon_cycles = 0;
  ctxt->send_cycles = 0;
  ctxt->send_handler_cycles = 0;
  ctxt->recv_cycles = 0;
  ctxt->recv_handler_cycles = 0;

  return;
}

#endif /* SOFTSWITCH_ENABLE_PROFILE */

/* 
   Handler calls
*/

//! handler exit call, terminates the executive with code as the return status
// WARNING THIS IS NOW DEPRICATED
extern "C" void softswitch_handler_exit(int code)
{
  hostMsg msg;
  msg.source.thread = tinselId(); // Id of this thread 
  msg.type = 0xFF; // magic number for exit
  msg.payload[0] = (uint32_t) code;
  // push onto the back of the buffer
  hostMsgBufferPush(&msg);
  //directHostMessageSlowSend(&msg); // send the assert via the UART bypassing the buffer
  return;
}

//! assertion function, terminates the current execution
// assertions are different because they do not use the host message buffer but send the message directly and then block.
// It should probably be sent up the UART (feels safer?)
extern "C" void __assert_func (const char *file, int line, const char *assertFunc,const char *cond)
{
  hostMsg msg;
  msg.source.thread = tinselId(); // Id of this thread 
  msg.type = 0xFE; // magic number for assert with no info
  msg.payload[0] = (unsigned)static_cast<const void*>(file); //address of the file where the assert occurred
  msg.payload[1] = line; // line number for where the assert occurred 
  msg.payload[2] = (unsigned)static_cast<const void*>(assertFunc); //string address for function where assert asserted 
  msg.payload[3] = (unsigned)static_cast<const void*>(cond); // string address of cond
  directHostMessageSlowSend(&msg); // send the assert via the UART bypassing the buffer
  while(1);
}
