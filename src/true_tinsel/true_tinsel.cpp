#include "tinsel.h"
#include "softswitch.hpp"
#include "softswitch_perfmon.hpp"
#include "tinsel_api.hpp"
#include "tinsel_api_shim.hpp"
#include "hostMsg.hpp"

#include <stddef.h>
#include <cstdarg>
#include <math.h>

extern "C" void * memcpy ( void * destination, const void * source, size_t num )
{
  // TODO: this is slow, but small
  for(unsigned i=0; i<num; i++){
    ((char*)destination)[i] = ((const char *)source)[i];
  }
  return destination;
}

extern "C" void *memset( void *dest, int ch, size_t count )
{
  uint8_t *pDst=(uint8_t*)dest;
  for(unsigned i=0; i<count; i++){
    *pDst=ch;
  }
  return dest;
}

extern "C" int strlen(const char *str)
{
  int len=0;
  while(*str++){
    ++len;
  }
  return len;
}

extern "C" char * strncpy ( char * destination, const char * source, size_t num )
{
  const char *upper=destination+num;
  char *curr=destination;
  while(curr<upper){
    char ch=*source;
    *curr=ch;
    ++curr;
    if(ch!=0){
      ++source;
    }
  }
  return destination;
}

extern "C" int strcmp(const char *a, const char *b)
{
  while(1){
    char ca=*a++;
    char cb=*b++;
    if(ca < cb){
      return -1;
    }
    if(ca > cb){
      return +1;
    }
    if(ca==0){
      return 0;
    }
  }
}

//#ifndef POETS_DISABLE_LOGGING
//
//extern "C" int vsnprintf_string( char * buffer, int bufsz, char pad, int width, const char *data)
//{
//  int done=0;
//
//  // Can write to [buffer,bufferMax)
//  char *bufferMax=buffer+bufsz-1;
//  
//  int len=strlen(data);
//
//  // TODO: padding and width
//  while(*data){
//    if(buffer<bufferMax){
//      *buffer++ = *data;
//    }
//    ++data;
//  }
//  
//  return len;
//}
//
//
//
//extern "C" int vsnprintf_hex( char * buffer, int bufsz, char pad, int width, unsigned val)
//{
//  
//  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");
//
//  static const char digits[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
//  
//  char tmp[16]={0};
//  int len=0;
//  
//  bool isZero=true;
//  for(int p=7;p>=0;p--){
//    int d=val>>28;
//    if(d>0 || !isZero){
//      tmp[len++]=digits[d];
//      isZero=false;
//    }
//    val=val<<4;
//  }
//  if(isZero){
//    tmp[0]='0';
//  }
//  
//  return vsnprintf_string(buffer, bufsz, pad, width, tmp);
//}
//
//uint32_t int2str(unsigned val, char *tmp)
//{
//  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");
//  
//  static const unsigned pow10[10]={
//           1,           10,         100,
//	   1000,        10000,      100000,
//	   1000000,     10000000,   100000000,
//	   1000000000ul
//  };
//  
//  uint32_t len=0;
//  
//  bool nonZero=false;
//  for(int p=sizeof(pow10)/sizeof(pow10[0])-1; p>=0; p--){
//    int d=0;
//    while( val > pow10[p] ){
//      val-=pow10[p];
//      d=d+1;
//    }
//    if(d>0 || nonZero){
//      tmp[len++]=d+'0';
//    }
//  }
//  if(!nonZero){
//    tmp[len++]='0';
//  }
//
//  return len;
//}
// 
//extern "C" int vsnprintf_float(char * buffer, int bufsz, char pad, int width, float fpnum)
//{
//   char fpnum_str[60];
//   char * fpnum_str_ptr = &fpnum_str[0];
//
//   // integer part
//   uint32_t ipart = (uint32_t)fpnum; // integer part
//   uint32_t l = int2str(ipart, fpnum_str);
//   *(fpnum_str_ptr + l) = '.';
//   l++;
//
//   // float part
//   float fpart = fpnum - (float)ipart; // float part
//   uint32_t fpart_int = (uint32_t)(100000000 * fpart); //9dp 
//   int2str(fpart_int, fpnum_str_ptr + l);
//   return vsnprintf_string(buffer, bufsz, pad, width, fpnum_str);
//}
//
//extern "C" int vsnprintf_unsigned( char * buffer, int bufsz, char pad, int width, unsigned val)
//{
//  // TODO: width
//  // TODO: zeroPad
//  
//  char tmp[16] = { 0 };
//  int2str(val, tmp);
//  return vsnprintf_string(buffer, bufsz, pad, width, tmp);
//}
//
//extern "C" int vsnprintf_signed( char * buffer, int bufsz, char pad, int width, int val)
//{
//  int done=0;
//  if(val<0){
//    if(bufsz>1){
//      *buffer++='-';
//      bufsz--;
//    }
//    done=1;
//  }
//  return done+vsnprintf_unsigned(buffer, bufsz, pad, width, (unsigned)-val);
//}
//
//extern "C" int isdigit(int ch)
//{
//  return '0'<=ch && ch <='9';
//}
//
//extern "C" int vsnprintf( char * buffer, int bufsz, const char * format, va_list vlist )
//{
//  /*
//  buffer[bufsz-1]=0;
//  strncpy(buffer, format, bufsz-1);
//  return strlen(format);
//  */
//  
//  memset(buffer, 0, bufsz);
//  
//  int done=0;
//
//  // We can write in [buffer,bufferMax)
//  char *bufferMax=buffer+bufsz-1;
//  
//  while(*format){
//    char ch=*format++;
//    int delta=0;
// 
//    if(ch=='%'){
//      int width=-1;
//      char padChar=' ';
//      char type=0;
//      
//      // flags
//      while(1){
//        if(*format=='0'){
//          padChar='0';
//          format++;
//        }else{
//          break;
//        }
//      }
//      
//      // Width
//      while(1){
//        if(isdigit(*format)){
//          if(width==-1)
//            width=0;
//          width=width*10+(*format-'0');
//          ++format;
//        }else{
//          break;
//        }
//      }
//      
//      type=*format++;
//      switch(type){
//	case 'u':
//        //delta=vsnprintf_unsigned(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
//        delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
//        break;
//      case 'd':
//        //delta=vsnprintf_signed(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,signed));
//        delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
//        break;
//      case 'x':
//        delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
//        break;
//      case 's':
//        delta=vsnprintf_string(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,const char *));
//        break;
//      case 'f' : 
//        delta=vsnprintf_float(buffer, (bufferMax-buffer)+1, padChar, width, bin2fp(va_arg(vlist, uint32_t)));
//        break;
//      case '%':
//	if(buffer<bufferMax){
//	  *buffer='%';
//	}
//	delta=1;
//	break;
//      default:
//	// Print back out minimal format string we didn't handle
//	if(buffer<bufferMax){
//	  *buffer='%';
//	}
//	if(buffer+1<bufferMax){
//	  buffer[1]=type;
//	}
//        delta=2;
//        break;
//      }
//    }else{
//      if(buffer<bufferMax){
//	*buffer=ch;
//      }
//      delta=1;
//    }
//    
//    done+=delta;
//    buffer=buffer+delta;
//    if(buffer>bufferMax){
//      buffer=bufferMax;
//    }
//  }
//
//  while(buffer <= bufferMax){
//    *buffer=0;
//    buffer++;
//  }
//  
//  return done;
//}
//
//#endif

/* Wrapper around the tinselUartTryPut() function, keeps trying until successful. 
*/
void tinsel_UartTryPut(uint8_t x) {
  while(!tinselUartTryPut(x)) { }
  return; 
}


/* Protocol: 

  We need to send data back up to the host, but each word needs to
  include the source (I think). So we'll use a 16-bit thread id.

  Each word consists of 16-bits of id (hi) and 16-bits of payload (lo)

  stdout:
  IIIIII01  // 1 = stdout
  IIIIIICC  // Payload in LSBS
  ...
  IIIIIICC
  IIIIII00  // Terminating NULL.

  stderr:
  IIIIII02  // 2 = stderr
  IIIIIICC  // Payload in LSBS
  ...
  IIIIIICC
  IIIIII00  // Terminating NULL.

  Assertion (with file name and line):
  IIIIIIFD  // Magic number for assertion
  IIIIIICC
  IIIIIICC
  ...
  IIIIIICC
  IIIIII00
  IIIIIILL  // 8-bits of line (LSB)
  IIIIIILL  // 8-bits of line
  IIIIIILL  // 8-bits of line
  IIIIIILL  // 8-bits of line (MSB)
 

  Assertion (with no further information):
  IIIIIIFE  // Magic number for assertion
  
  
  Exit:
  IIIIIIFF  // Magic number for exit
  IIIIIIVV  // 8-bits of value (LSB)
  IIIIIIVV  // 8-bits of value
  IIIIIIVV  // 8-bits of value
  IIIIIIVV  // 8-bits of value (MSB)

  Pair of 32-bit values from a device
  IIIIII10  // Magic number
  IIIIIICC  // first char of id
  IIIIIICC  // second char of id
  ...
  IIIIII00  // terminating null of id
  IIIIIIKK  // 8-bits of key (LSB)
  IIIIIIKK  // 8-bits of key
  IIIIIIKK  // 8-bits of key
  IIIIIIKK  // 8-bits of key (MSB)
  IIIIIIVV  // 8-bits of value (LSB)
  IIIIIIVV  // 8-bits of value
  IIIIIIVV  // 8-bits of value
  IIIIIIVV  // 8-bits of value (MSB)

  Performance Counter Flush
  IIIIII20  // Magic number for perfmon flush
  IIIIIITC  // 8-bits of total cycles (LSB)
  IIIIIITC  // 8-bits of total cycles 
  IIIIIITC  // 8-bits of total cycles 
  IIIIIITC  // 8-bits of total cycles (MSB)
  IIIIIIBC  // 8-bits of blocked cycles (LSB)
  IIIIIIBC  // 8-bits of blocked cycles 
  IIIIIIBC  // 8-bits of blocked cycles 
  IIIIIIBC  // 8-bits of blocked cycles (MSB)
  IIIIIIDC  // 8-bits of idle cycles (LSB)
  IIIIIIDC  // 8-bits of idle cycles 
  IIIIIIDC  // 8-bits of idle cycles 
  IIIIIIDC  // 8-bits of idle cycles (MSB)
  IIIIIIPC  // 8-bits of perfmon cycles (LSB)
  IIIIIIPC  // 8-bits of perfmon cycles 
  IIIIIIPC  // 8-bits of perfmon cycles 
  IIIIIIPC  // 8-bits of perfmon cycles (MSB)
  IIIIIISC  // 8-bits of send cycles (LSB)
  IIIIIISC  // 8-bits of send cycles 
  IIIIIISC  // 8-bits of send cycles 
  IIIIIISC  // 8-bits of send cycles (MSB)
  IIIIIISH  // 8-bits of send handler cycles (LSB)
  IIIIIISH  // 8-bits of send handler cycles 
  IIIIIISH  // 8-bits of send handler cycles 
  IIIIIISH  // 8-bits of send handler cycles (MSB)
  IIIIIIRC  // 8-bits of recv cycles (LSB)
  IIIIIIRC  // 8-bits of recv cycles 
  IIIIIIRC  // 8-bits of recv cycles 
  IIIIIIRC  // 8-bits of recv cycles (MSB)
  IIIIIIRH  // 8-bits of recv handler cycles (LSB)
  IIIIIIRH  // 8-bits of recv handler cycles 
  IIIIIIRH  // 8-bits of recv handler cycles 
  IIIIIIRH  // 8-bits of recv handler cycles (MSB)

*/

// txt the message to be sent over stdout
extern "C" void tinsel_puts(const char *txt){

//  // Get the thread and device contexts
//  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
//  
//  //Each message uses 2 flits
//  tinselSetLen(HOSTMSG_FLIT_SIZE);
//
//  //get host id
//  int host = tinselHostId();
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(HOSTMSG_MBOX_SLOT);
//  msg->id = tinselId();  
//
////  msg->payload[0] = 0x01; //magic number for stdout
////  uint32_t c = 1;
////  while(*txt) {
////     // increment c, if we have filled the packet send it 
////     if(c < HOST_MSG_PAYLOAD) {
////         msg->payload[c] = uint8_t(*txt);
////         txt++;
////         c++;
////     } else {
////         msg->size = c;
////         tinselSend(host,msg);
////         tinselWaitUntil(TINSEL_CAN_SEND);
////         c = 0;
////     }
////  }
////  
////  // reached the end of the txt stream, sending null terminator
////  if(c < HOST_MSG_PAYLOAD) {
////    //There is enough space in the current packet 
////    msg->payload[c] = '\0';
////    msg->size = c;
////    tinselSend(host,msg);
////  } else {
////    //not enough space
////    
////    //send the remaining data
////    tinselSend(host,msg);
////    tinselWaitUntil(TINSEL_CAN_SEND);
////
////    //message containing just the null terminator
////    msg->size = 1;
////    msg->payload[0] = '\0';
////
////    //send the null terminator
////    tinselSend(host,msg);
////  }
//
//
//  //keep track of if this is the first message sent (for magic number)
//  uint8_t first = 1;
//   
//  //keep adding txt to payload until its gone
//  while(*txt) {
//
//    uint8_t size = 0;
//    // prepare the payload
//    for(uint8_t i=0; i<HOST_MSG_PAYLOAD; i++) {
//       if(i==0 && first) {
//          msg->payload[0] = 0x01; //magic number for stdout
//          first = 0; //no longer the first message
//       } else {
//          msg->payload[i] = uint8_t(*txt);
//          if(!*txt) {
//            break;
//          } 
//          txt++;
//       }
//       size++;
//    }    
//    msg->size = size; 
//    //tinselSend(host,msg);
//    
//    //send terminating null
//    if(!*txt) {
//      if(size < HOST_MSG_PAYLOAD) {
//        //There is space to send the terminator
//        msg->payload[size+1] = '\0';
//        msg->size = size + 1;
//        //send the message    
//        tinselSend(host, msg); 
//        return;
//      } else {
//        //There is no space in the current message
//        //So send an additional message
//
//        //send the current message    
//        tinselSend(host, msg); 
//        msg->size = 1;
//        msg->payload[0] = '\0'; 
//        //send the message containing only the terminator
//        tinselSend(host, msg); 
//        return;
//      }
//    } else {
//      //send the message    
//      tinselSend(host, msg); 
//    }
// 
//    //move onto the next chunk of the message
//  }
//
//  // restore message size before returning
//  tinsel_mboxSetLen(ctxt->currentSize);

  return; 
}


//#ifdef SOFTSWITCH_ENABLE_PROFILE
//extern "C" void softswitch_flush_perfmon() {

//  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
//
//  uint32_t thread_cycles = ctxt->thread_cycles;
//  uint32_t blocked_cycles = ctxt->blocked_cycles;
//  uint32_t idle_cycles = ctxt->idle_cycles;
//  uint32_t perfmon_cycles = ctxt->perfmon_cycles;
//  uint32_t send_cycles = ctxt->send_cycles;
//  uint32_t send_handler_cycles = ctxt->send_handler_cycles;
//  uint32_t recv_cycles = ctxt->recv_cycles;
//  uint32_t recv_handler_cycles = ctxt->recv_handler_cycles;
//
//  //Each message uses 1 flit
//  tinselSetLen(HOSTMSG_FLIT_SIZE);
//
//  //get host id
//  int host = tinselHostId();
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(HOSTMSG_MBOX_SLOT);
//  msg->id = tinselId();  
//  msg->size = 13;
//
//  msg->payload[0] = 0x20; //Magic value for perfmon data
//
//  msg->payload[1] = ((thread_cycles>>0)&0xFF);
//  msg->payload[2] = ((thread_cycles>>8)&0xFF);
//  msg->payload[3] = ((thread_cycles>>16)&0xFF);
//  msg->payload[4] = ((thread_cycles>>24)&0xFF);
//
//  msg->payload[5] = ((blocked_cycles>>0)&0xFF);
//  msg->payload[6] = ((blocked_cycles>>8)&0xFF);
//  msg->payload[7] = ((blocked_cycles>>16)&0xFF);
//  msg->payload[8] = ((blocked_cycles>>24)&0xFF);
//
//  msg->payload[9] = ((idle_cycles>>0)&0xFF);
//  msg->payload[10] = ((idle_cycles>>8)&0xFF);
//  msg->payload[11] = ((idle_cycles>>16)&0xFF);
//  msg->payload[12] = ((idle_cycles>>24)&0xFF);
//
//  // send a message
//  tinselSend(host, msg); 
//  
//  //wait until we can send
//  tinselWaitUntil(TINSEL_CAN_SEND);
//
//  msg->size = 13; 
//
//  msg->payload[0] = ((perfmon_cycles>>0)&0xFF);
//  msg->payload[1] = ((perfmon_cycles>>8)&0xFF);
//  msg->payload[2] = ((perfmon_cycles>>16)&0xFF);
//  msg->payload[3] = ((perfmon_cycles>>24)&0xFF);
//
//  msg->payload[4] = ((send_cycles>>0)&0xFF);
//  msg->payload[5] = ((send_cycles>>8)&0xFF);
//  msg->payload[6] = ((send_cycles>>16)&0xFF);
//  msg->payload[7] = ((send_cycles>>24)&0xFF);
//
//  msg->payload[8] = ((send_handler_cycles>>0)&0xFF);
//  msg->payload[9] = ((send_handler_cycles>>8)&0xFF);
//  msg->payload[10] = ((send_handler_cycles>>16)&0xFF);
//  msg->payload[11] = ((send_handler_cycles>>24)&0xFF);
//
//  msg->payload[12] = ((recv_cycles>>0)&0xFF);
//
//  //send the message    
//  tinselWaitUntil(TINSEL_CAN_SEND);
//  tinselSend(host, msg); 
//  
//  //wait until we can send
//  tinselWaitUntil(TINSEL_CAN_SEND);
//
//  msg->size = 7; 
//
//  msg->payload[0] = ((recv_cycles>>8)&0xFF);
//  msg->payload[1] = ((recv_cycles>>16)&0xFF);
//  msg->payload[2] = ((recv_cycles>>24)&0xFF);
//
//  msg->payload[3] = ((recv_handler_cycles>>0)&0xFF);
//  msg->payload[4] = ((recv_handler_cycles>>8)&0xFF);
//  msg->payload[5] = ((recv_handler_cycles>>16)&0xFF);
//  msg->payload[6] = ((recv_handler_cycles>>24)&0xFF);
//
//  //send the message    
//  tinselWaitUntil(TINSEL_CAN_SEND);
//  tinselSend(host, msg); 
//
//  //Reset the performance counters
//  ctxt->thread_cycles = 0;
//  ctxt->blocked_cycles = 0;
//  ctxt->idle_cycles = 0;
//  ctxt->perfmon_cycles = 0;
//  ctxt->send_cycles = 0;
//  ctxt->send_handler_cycles = 0;
//  ctxt->recv_cycles = 0;
//  ctxt->recv_handler_cycles = 0;
//
//  // restor the message size
//  tinsel_mboxSetLen(ctxt->currentSize);

//}
//#endif

// code the exit code for the application
//extern "C" void softswitch_handler_exit(int code)
//{
//  // get the context for the thread and device
//  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
//
//  tinsel_mboxSetLen(sizeof(hostMsg));
//
//  //get host id
//  int host = tinselHostId();
//
//  //wait until we can send
//  // tinselWaitUntil(TINSEL_CAN_SEND);
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(1); // slot 1 is reserved for host messages
//  msg->id = tinselId();  
//
//  //Add the payload
//  msg->type = 0x0F; //magic number for exit
//  msg->parameters[0] = (uint32_t)code;
//
//  //send the message    
//  tinselSend(host, msg); 
//  
//  return;
//  // get the context for the thread and device
//  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
//
//  //Each message uses 1 flit
//  tinselSetLen(HOSTMSG_FLIT_SIZE);
//
//  //get host id
//  int host = tinselHostId();
//
//  //wait until we can send
//  tinselWaitUntil(TINSEL_CAN_SEND);
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(HOSTMSG_MBOX_SLOT);
//  msg->id = tinselId();  
//
//  //Add the payload
//  uint32_t key=uint32_t(code);
//  msg->size = 5;
//  msg->payload[0] = 0xFF; //Magic value for exit 
//  msg->payload[1] = ((key>>0)&0xFF); 
//  msg->payload[2] = ((key>>8)&0xFF); 
//  msg->payload[3] = ((key>>16)&0xFF);
//  msg->payload[4] = ((key>>24)&0xFF);
//  msg->payload[5] = 0;
//  msg->payload[6] = 0;
//  msg->payload[7] = 0;
//
//  //send the message    
//  tinselWaitUntil(TINSEL_CAN_SEND);
//  tinselSend(host, msg); 
//
//  //Restore message size
//  tinsel_mboxSetLen(ctxt->currentSize);
//  
//  return;
//}

// key the key to be exported
// the associated value with the key
extern "C" void softswitch_handler_export_key_value(uint32_t key, uint32_t value)
{
//  // get the context for the thread and device
//  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
// 
//  //Each message uses 1 flit
//  tinselSetLen(HOSTMSG_FLIT_SIZE);
//
//  //get host id
//  int host = tinselHostId();
//
//  //wait until we can send
//  tinselWaitUntil(TINSEL_CAN_SEND);
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(HOSTMSG_MBOX_SLOT);
//  msg->id = tinselId();  
//
//  //Add the payload
//  msg->size = 9;
//  msg->payload[0] = 0x10; //magic value keyvalue export 
//  msg->payload[1] = ((key>>0)&0xFF); 
//  msg->payload[2] = ((key>>8)&0xFF); 
//  msg->payload[3] = ((key>>16)&0xFF);
//  msg->payload[4] = ((key>>24)&0xFF);
//  msg->payload[5] = ((value>>0)&0xFF);
//  msg->payload[6] = ((value>>8)&0xFF);
//  msg->payload[7] = ((value>>16)&0xFF);
//  msg->payload[8] = ((value>>24)&0xFF);
//  msg->payload[9] = 0;
//
//  //send the message    
//  //tinselSend(host, msg); 
//  
//  //Restore message size
//  tinsel_mboxSetLen(ctxt->currentSize);
//  
  return;
}

//extern "C" void __assert_func (const char *file, int line, const char *assertFunc,const char *cond)
//{
//  //Each message uses 1 flit
//  tinselSetLen(HOSTMSG_FLIT_SIZE);
//
//  //get host id
//  int host = tinselHostId();
//
//  //wait until we can send
//  tinselWaitUntil(TINSEL_CAN_SEND);
//
//  //prepare the message 
//  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(HOSTMSG_MBOX_SLOT);
//  msg->id = tinselId();  
//  uint16_t msgsize = 1;
//
//  #ifndef SOFTSWITCH_MINIMAL_ASSERT
//  msg->payload[0] = 0xFD; // magic number for assert 
//
//  while(1){
//    char ch=*file++;
//    msg->payload[msgsize] = uint8_t(ch);
//    
//    if(ch==0){
//       break;
//    }
//
//    //if we are at the msgpayload size we need to transmit
//    if(msgsize == HOST_MSG_PAYLOAD) {
//       msg->size = msgsize;
//
//       // send the message
//       tinselWaitUntil(TINSEL_CAN_SEND);
//       tinselSend(host, msg);
//       
//       //wait until we can send
//       tinselWaitUntil(TINSEL_CAN_SEND); 
//    }
//    msgsize++;
//  }
//  msg->size = msgsize;
//  
//  // send the message
//  tinselWaitUntil(TINSEL_CAN_SEND);
//  tinselSend(host, msg);
//
//  #else 
//
//  msg->payload[0] = 0xFE; // magic number for assert 
//  msg->size = 1;
//
//  // send the message
//  tinselWaitUntil(TINSEL_CAN_SEND);
//  tinselSend(host, msg);
//  #endif
//
//  tinsel_mboxWaitUntil((tinsel_WakeupCond)0);
//    while(1);
//    return;
//}

int main()
{
  softswitch_main();
}
