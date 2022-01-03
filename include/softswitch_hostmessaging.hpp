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
    // check to ensure that the hostMessage buffer has enough space (otherwise pop via UART)
    if(hostMsgBufferSpace() == 0) {
      hostMessageSlowPopSend();
    }

    // push the message onto the hostMsgBuffer
    //hostMsgBufferPush(hmsg);
    directHostMessageSlowSend(hmsg);

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
void softswitch_handler_log_impl(int level, uint32_t pc, const char *msg, const Param& ... param){

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
  hmsg.source.device = deviceContext->index;
  hmsg.type = 0xF0; // magic number for STDOUT

  // String address of the message
  hmsg.payload[0] = (unsigned)static_cast<const void*>(msg);

  // current handler type TODO: this can be worked out by pts-serve using src addr
  hmsg.payload[1] = (pc << 8) | ctxt->currentHandlerType;
  // pin name
  const char* pin;
  if(ctxt->currentHandlerType==1){
    pin=deviceContext->vtable->inputPins[ctxt->currentPin].name;
  }else if(ctxt->currentHandlerType==2){
    pin=deviceContext->vtable->outputPins[ctxt->currentPin].name;
  }
  hmsg.payload[2] = (unsigned)static_cast<const void*>(pin); // send the str addr of the pin name
  hmsg.payload[3] = (unsigned)static_cast<const void*>(deviceContext->id); // send the str addr of the device name
  // TODO: above could be done more efficiently if pts-serve looked this up instead of sending the message

  // peel off the parameters from the variadic
  uint32_t param_cnt = 4;
  log_peel_params(&hmsg, &param_cnt, param...);
  assert(param_cnt <= HOST_MSG_PAYLOAD);

  return;
}

#ifndef POETS_DISABLE_LOGGING

template<typename T = void>
void log_peel_params_ss(hostMsg *hmsg, uint32_t* cnt) {
    directHostMessageSlowSend(hmsg);

    return;
}

// peel parameters
// General case: peels the parameters off the argument list and appends them to the message
template<typename P1, typename ... Param>
void log_peel_params_ss(hostMsg *hmsg, uint32_t* cnt, const P1& p1, Param& ... param){
    hmsg->payload[*cnt] = *((uint32_t*)&p1); //prune type and place it in hostMsg parameters
    *cnt = *cnt + 1;
    log_peel_params_ss(hmsg, cnt, param ...); //keep peeling
    return;
}

//! The softswith log messaging (this is for debugging, it uses the slower UART channel as otherwise a deadlock might occur)
// extern "C" void softswitch_softswitch_log_impl(int level, const char *msg, ...)
template<typename ... Param>
void softswitch_softswitch_log_impl(int level, uint32_t pc, const char *msg, const Param& ... param){

  PThreadContext *ctxt=softswitch_getCtxt();

  if(level > ctxt->applLogLevel)
    return;

  const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;

  // create a hostMsg
  hostMsg hmsg;

  //prepare the message
  hmsg.source.thread = tinselId();
  hmsg.source.device = 0xFFFF;
  hmsg.type = 0xF0; // magic number for STDOUT
  hmsg.payload[0] = (unsigned)static_cast<const void*>(msg);
  hmsg.payload[1]=(pc<<8);
  hmsg.payload[2]=0;
  hmsg.payload[3]=0;

  // peel off the parameters from the variadic
  uint32_t param_cnt = 4;
  log_peel_params_ss(&hmsg, &param_cnt, param...);
  assert(param_cnt <= HOST_MSG_PAYLOAD);
  return;
}
#endif /* POETS_DISABLE_LOGGING */

#endif /* SOFTSWITCH_HOSTMESSAGING */
