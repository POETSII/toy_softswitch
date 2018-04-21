#ifndef SOFTSWITCH_HOSTMESSAGING
#define SOFTSWITCH_HOSTMESSAGING
// softswitch_hostmessaging.cpp

// This file contains functions used in sending messages to the host in tinsel-0.3
// It contains several function for managing the host message buffer.
// It also contains some variadic template functions for constructing host messages

//#include "softswitch_hostmessaging.hpp"
#include "tinsel_api.hpp"
#include <stdint.h>
#include <vector>
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

//! directHostMessageSlowSend
// bypasses the buffer and the main softswitch event loop and sends the hostmessage directly
// this is used to send asserts etc.. where the rest of the softswitch should stop executing.
void directHostMessageSlowSend(hostMsg *msg);

/*
   Handler messages (non-variadic)
*/

//! handler exit call, terminates the executive with code as the return status
void softswitch_handler_exit(int code);

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
template<typename P1, typename ... Param>
void log_peel_params(hostMsg *hmsg, uint32_t* cnt, const P1& p1, Param& ... param){
    hmsg->payload[*cnt] = *((uint32_t*)&p1); //prune type and place it in hostMsg parameters
    *cnt = *cnt + 1;
    log_peel_params(hmsg, cnt, param ...); //keep peeling
    return;
}

//! to_raw_binary: Converts any parameter type into raw binary that can be packed into the payload
// (type information is stored in the unparameterised string at the host end)
//template<typename T>
//uint32_t to_raw_binary(const T& t){
//  return *((uint32_t*)&t); 
//}
//
////! unpeel_all_params: unpeels all the parameters of the log function call and stores them in a vector
//template<typename ... Param>
//std::vector<uint32_t> unpeel_all_params(const Param& ... param){
//  return {to_raw_binary(param)...};
//} 

//! The log message call
template<typename ... Param>
void softswitch_handler_log_impl(int level, const char *msg, const Param& ... param){

  PThreadContext *ctxt=softswitch_getCtxt();
  assert(ctxt->currentHandlerType!=0); // can't call handler log when not in a handler
  assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

  if(level > ctxt->applLogLevel)
    return;

  const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;

  // create a hostMsg 
  hostMsg hmsg;

  //prepare the message
  hmsg.source.thread = tinselId();
  hmsg.source.device = 0; // TODO: make thi the actual device
  hmsg.type = 0x1; // magic number for STDOUT
  hmsg.payload[0] = (unsigned)static_cast<const void*>(msg);

  // peel off the parameters from the variadic
  uint32_t param_cnt = 1;
  log_peel_params(&hmsg, &param_cnt, param...);
  assert(param_cnt <= HOST_MSG_PAYLOAD);

  //std::vector<uint32_t> params = unpeel_all_params(param...);
  //assert(params.size() <= HOST_MSG_PAYLOAD);
  //for(uint32_t i=0; i<params.size(); i++) {
  //  hmsg.payload[i] = params[i]; 
  //}

  // push the message onto the hostMsgBuffer
  hostMsgBufferPush(&hmsg);

  return;
}

#ifndef POETS_DISABLE_LOGGING

//! The softswith log messaging (this is for debugging, it uses the slower UART channel as otherwise a deadlock might occur)
// extern "C" void softswitch_softswitch_log_impl(int level, const char *msg, ...)
template<typename ... Param>
void softswitch_softswitch_log_impl(int level, const char *msg, const Param& ... param){

  PThreadContext *ctxt=softswitch_getCtxt();
  assert(ctxt->currentHandlerType!=0); // can't call handler log when not in a handler
  assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

  if(level > ctxt->applLogLevel)
    return;

  const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;

  // create a hostMsg 
  hostMsg hmsg;

  //prepare the message
  hmsg.source.thread = tinselId();
  hmsg.source.device = 0; //TODO: make this the actual device index
  hmsg.type = 0x1; // magic number for STDOUT
  hmsg.payload[0] = (unsigned)static_cast<const void*>(msg);

  // peel off the parameters from the variadic
  uint32_t param_cnt = 1;
  log_peel_params(&hmsg, &param_cnt, param...);
  assert(params_cnt <= HOST_MSG_PAYLOAD);

  //std::vector<uint32_t> params = unpeel_all_params(param...);
  //assert(params.size() <= HOST_MSG_PAYLOAD);
  //for(uint32_t i=0; i<params.size(); i++) {
  //  hmsg.payload[i] = params[i]; 
  //}

  // This does not go through the buffer as it could cause deadlock
  directHostMessageSlowSend(&hmsg);

  return;
}
#endif /* POETS_DISABLE_LOGGING */

#endif /* SOFTSWITCH_HOSTMESSAGING */
