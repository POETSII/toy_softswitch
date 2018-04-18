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
  buff[ctxt->hbuf_tail].id = msg->id;
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

//! handler exit call, terminates the executive with code as the return status
extern "C" void softswitch_handler_exit(int code)
{
  hostMsg msg;
  msg.id = tinselId(); // Id of this thread 
  msg.type = 0x0F; // magic number for exit
  msg.payload[0] = (uint32_t) code;
  hostMsgBufferPush(&msg); // push the exit code to the back of the queue
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
  hmsg->id = buff[ctxt->hbuf_head].id;
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

