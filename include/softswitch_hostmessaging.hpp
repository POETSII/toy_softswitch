#ifndef SOFTSWITCH_HOSTMESSAGING
#define SOFTSWITCH_HOSTMESSAGING
// softswitch_hostmessaging.cpp

// This file contains functions used in sending messages to the host in tinsel-0.3
// It contains several function for managing the host message buffer.
// It also contains some variadic template functions for constructing host messages

//#include "softswitch_hostmessaging.hpp"
#include "tinsel_api.hpp"
#include <stdint.h>
#include "hostMsg.hpp"
#include "softswitch.hpp"

// defines the Host Message Buffer size that is defined in softswitch_main
#define HOSTBUFFER_SIZE 40 
#define MAX_HOST_PER_HANDLER 2 

/*
    Host Message Buffer functions
*/
// gets the context for the current thread, this is where host message buffer pointer is stored
static PThreadContext *softswitch_getCtxt()
{
  if(tinsel_myId() < softswitch_pthread_count){
    return softswitch_pthread_contexts + tinsel_myId();
  }else{
    return 0;
  }
}

//! returns the size of the buffer (i.e. number of pending host messages)
extern "C" uint32_t hostMsgBufferSize();

//! returns the space available in the buffer 
extern "C" uint32_t hostMsgBufferSpace();

//! Pushes a host message onto the buffer
extern "C" void hostMsgBufferPush(hostMsg *msg);

//! slow hostlink - sends data up the hostlink instead of via PCIe messages
// pops the data off the buffer and passes it up the UART, can be used in situations where the buffer is full
// and we don't want to block
extern "C" void hostMessageSlowPopSend(); 

//! pops and sends a message from the buffer
extern "C" void hostMessageBufferPopSend(); 

/*
    Variadic host log message calls
*/

// Peels the parameters from the variadic argument and type-prunes for transfer to host

// peel parameters
// zeroth case: returns
template<typename T = void>
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
void softswitch_handler_log(int level, const char *msg, const Param& ... param){

  //Building a temporary handler log for playing about with this
  PThreadContext *ctxt=softswitch_getCtxt();
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

#endif /* SOFTSWITCH_HOSTMESSAGING */
