// softswitch_hostmessaging.cpp

// This file contains functions used in sending messages to the host in tinsel-0.3
// It contains several function for managing the host message buffer.
// It also contains some variadic template functions for constructing host messages

#include "softswitch.hpp"
#include "tinsel_api.h"

// gets the context for the current thread, this is where host message buffer pointer is stored
// defined in softswitch_main.cpp
extern "C" static PThreadContext *softswitch_getContext();

/*
    Host Message Buffer functions
*/

//! returns the size of the buffer (i.e. number of pending host messages)
uint32_t hostMsgBufferSize() {
  PThreadContext *ctxt=softswitch_getContext();
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

//! slow hostlink - sends data up the hostlink instead of via PCIe messages
// pops the data off the buffer and passes it up the UART, can be used in situations where the buffer is full
// and we don't want to block
void hostMessageSlowPopSend() {
  // Protocol for sending host messages over this channel
  // 16-bits of ID (hi) and 8-bits of payload (lo)
  // TODO: Implement this
}


//! pops and sends a message from the buffer
void hostMessageBufferPopSend() {
  PThreadContext *ctxt=softswitch_getContext();

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

  return;
}

/*
    Variadic host log message calls
*/

// Peels the parameters from the variadic argument and type-prunes for transfer to host

// peel parameters
// zeroth case: returns
void log_peel_params(hostMsg *hmsg, uint32_t* cnt) {
    return;
}

// peel parameters
// General case: peels the parameters off the argument list and appends them to the message
// TODO: change this so that it uses initialisers to consume less code space
template<typename P1, typename ... Param>
void log_peel_params(hostMsg *hmsg, uint32_t* cnt, const P1& p1, Param& ... param){
    hmsg->parameters[*cnt] = *((uint32_t*)&p1); //prune type and place it in hostMsg parameters
    *cnt = *cnt + 1;
    log_peel_params(hmsg, cnt, param ...); //keep peeling
    return;
}


//! The log message call
template<typename ... Param>
void softswitch_handler_log_impl(int level, const char *msg, const Param& ... param){

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
  hmsg.type = 0x1; // magic number for STDOUT
  hmsg.strAddr = (unsigned)static_cast<const void*>(msg);

  // peel off the parameters from the variadic
  uint32_t param_cnt = 0;
  log_peel_params(&hmsg, &param_cnt, param...);

  // Temporary solution to check that we have enough space in the host message
  // currently just drops messages if they exceed the amount.
  // TODO:should be turned into an assertion once that support has been added.
  if(param_cnt > HOST_MSG_PAYLOAD)
    param_cnt = HOST_MSG_PAYLOAD;

  // push the message onto the hostMsgBuffer
  hostMsgBufferPush(&hmsg);

  return;
}
