#include "tinsel.h"
#include "softswitch.hpp"
#include "softswitch_perfmon.hpp"
#include "tinsel_api.hpp"
#include "tinsel_api_shim.hpp"

#include <stddef.h>
#include <cstdarg>
#include <math.h>

#define HOST_MSG_PAYLOAD 8 //TODO: This should really only be defined in one place
//Format of messages recv to the host
typedef struct {
  uint8_t padding;
  uint32_t id;
  uint8_t size;
  uint8_t payload[HOST_MSG_PAYLOAD];
} hostMsg;

inline float bin2fp(uint32_t f) { return *((float*) &f); }

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

#ifndef POETS_DISABLE_LOGGING

extern "C" int vsnprintf_string( char * buffer, int bufsz, char pad, int width, const char *data)
{
  int done=0;

  // Can write to [buffer,bufferMax)
  char *bufferMax=buffer+bufsz-1;
  
  int len=strlen(data);

  // TODO: padding and width
  while(*data){
    if(buffer<bufferMax){
      *buffer++ = *data;
    }
    ++data;
  }
  
  return len;
}



extern "C" int vsnprintf_hex( char * buffer, int bufsz, char pad, int width, unsigned val)
{
  
  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");

  static const char digits[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  
  char tmp[16]={0};
  int len=0;
  
  bool isZero=true;
  for(int p=7;p>=0;p--){
    int d=val>>28;
    if(d>0 || !isZero){
      tmp[len++]=digits[d];
      isZero=false;
    }
    val=val<<4;
  }
  if(isZero){
    tmp[0]='0';
  }
  
  return vsnprintf_string(buffer, bufsz, pad, width, tmp);
}

uint32_t int2str(unsigned val, char *tmp)
{
  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");
  
  static const unsigned pow10[10]={
           1,           10,         100,
	   1000,        10000,      100000,
	   1000000,     10000000,   100000000,
	   1000000000ul
  };
  
  uint32_t len=0;
  
  bool nonZero=false;
  for(int p=sizeof(pow10)/sizeof(pow10[0])-1; p>=0; p--){
    int d=0;
    while( val > pow10[p] ){
      val-=pow10[p];
      d=d+1;
    }
    if(d>0 || nonZero){
      tmp[len++]=d+'0';
    }
  }
  if(!nonZero){
    tmp[len++]='0';
  }

  return len;
}
 
extern "C" int vsnprintf_float(char * buffer, int bufsz, char pad, int width, float fpnum)
{
   char fpnum_str[60];
   char * fpnum_str_ptr = &fpnum_str[0];

   // integer part
   uint32_t ipart = (uint32_t)fpnum; // integer part
   uint32_t l = int2str(ipart, fpnum_str);
   *(fpnum_str_ptr + l) = '.';
   l++;

   // float part
   float fpart = fpnum - (float)ipart; // float part
   uint32_t fpart_int = (uint32_t)(100000000 * fpart); //9dp 
   int2str(fpart_int, fpnum_str_ptr + l);
   return vsnprintf_string(buffer, bufsz, pad, width, fpnum_str);
}

extern "C" int vsnprintf_unsigned( char * buffer, int bufsz, char pad, int width, unsigned val)
{
  // TODO: width
  // TODO: zeroPad
  
  char tmp[16] = { 0 };
  int2str(val, tmp);
  return vsnprintf_string(buffer, bufsz, pad, width, tmp);
}

extern "C" int vsnprintf_signed( char * buffer, int bufsz, char pad, int width, int val)
{
  int done=0;
  if(val<0){
    if(bufsz>1){
      *buffer++='-';
      bufsz--;
    }
    done=1;
  }
  return done+vsnprintf_unsigned(buffer, bufsz, pad, width, (unsigned)-val);
}

extern "C" int isdigit(int ch)
{
  return '0'<=ch && ch <='9';
}

extern "C" int vsnprintf( char * buffer, int bufsz, const char * format, va_list vlist )
{
  /*
  buffer[bufsz-1]=0;
  strncpy(buffer, format, bufsz-1);
  return strlen(format);
  */
  
  memset(buffer, 0, bufsz);
  
  int done=0;

  // We can write in [buffer,bufferMax)
  char *bufferMax=buffer+bufsz-1;
  
  while(*format){
    char ch=*format++;
    int delta=0;
 
    if(ch=='%'){
      int width=-1;
      char padChar=' ';
      char type=0;
      
      // flags
      while(1){
        if(*format=='0'){
          padChar='0';
          format++;
        }else{
          break;
        }
      }
      
      // Width
      while(1){
        if(isdigit(*format)){
          if(width==-1)
            width=0;
          width=width*10+(*format-'0');
          ++format;
        }else{
          break;
        }
      }
      
      type=*format++;
      switch(type){
	case 'u':
        delta=vsnprintf_unsigned(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
	//delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
        break;
      case 'd':
        delta=vsnprintf_signed(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,signed));
	//delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,signed));
        break;
      case 'x':
        delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
        break;
      case 's':
        delta=vsnprintf_string(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,const char *));
        break;
      case 'f' : 
        delta=vsnprintf_float(buffer, (bufferMax-buffer)+1, padChar, width, bin2fp(va_arg(vlist, uint32_t)));
        break;
      case '%':
	if(buffer<bufferMax){
	  *buffer='%';
	}
	delta=1;
	break;
      default:
	// Print back out minimal format string we didn't handle
	if(buffer<bufferMax){
	  *buffer='%';
	}
	if(buffer+1<bufferMax){
	  buffer[1]=type;
	}
        delta=2;
        break;
      }
    }else{
      if(buffer<bufferMax){
	*buffer=ch;
      }
      delta=1;
    }
    
    done+=delta;
    buffer=buffer+delta;
    if(buffer>bufferMax){
      buffer=bufferMax;
    }
  }

  while(buffer <= bufferMax){
    *buffer=0;
    buffer++;
  }
  
  return done;
}

#endif

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

// Print a string back via stdout channel (i.e. hostlink)
//extern "C" void tinsel_puts(const char *msg){
//  uint32_t prefix=tinselId()<<8;
//  tinsel_UartTryPut(prefix | 1);
//  while(1){
//    tinsel_UartTryPut(prefix | uint32_t(uint8_t(*msg)));
//    if(!*msg){
//      break;
//    }
//    msg++;
//  }
//}


extern "C" void tinsel_puts(const char *txt){
  
  //Each message uses 2 flits
  tinsel_mboxSetLen(0);

  //get host id
  int host = tinselHostId();

  //prepare the message 
  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(0);
  msg->id = tinselId();  

  //keep track of if this is the first message sent (for magic number)
  uint8_t first = 1;

  //keep adding txt to payload until its gone
  while(*txt) {

    uint8_t size = 0;
    // prepare the payload
    for(uint8_t i=0; i<HOST_MSG_PAYLOAD; i++) {
       if(i==0 && first) {
          msg->payload[0] = 0x01; //magic number for stdout
          first = 0; //no longer the first message
       } else {
          msg->payload[i] = uint8_t(*txt);
          if(!*txt) {
            break;
          } 
          txt++;
       }
       size++;
    }    
    msg->size = size; 

    //send the message    
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(host, msg); 
 
    //move onto the next chunk of the message
  }

  return; 
}


#ifdef SOFTSWITCH_ENABLE_PROFILE
extern "C" void softswitch_flush_perfmon() {
  uint32_t prefix=tinselId()<<8;
  
  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;

  tinsel_UartTryPut(prefix | 0x20); // Magic value for performance counter flush

  uint32_t thread_cycles = ctxt->thread_cycles;
  uint32_t blocked_cycles = ctxt->blocked_cycles;
  uint32_t idle_cycles = ctxt->idle_cycles;
  uint32_t perfmon_cycles = ctxt->perfmon_cycles;
  uint32_t send_cycles = ctxt->send_cycles;
  uint32_t send_handler_cycles = ctxt->send_handler_cycles;
  uint32_t recv_cycles = ctxt->recv_cycles;
  uint32_t recv_handler_cycles = ctxt->recv_handler_cycles;

  tinsel_UartTryPut(prefix | ((thread_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((thread_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((thread_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((thread_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((blocked_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((blocked_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((blocked_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((blocked_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((idle_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((idle_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((idle_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((idle_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((perfmon_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((perfmon_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((perfmon_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((perfmon_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((send_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((send_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((send_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((send_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((send_handler_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((send_handler_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((send_handler_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((send_handler_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((recv_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_cycles>>24)&0xFF));

  tinsel_UartTryPut(prefix | ((recv_handler_cycles>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_handler_cycles>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_handler_cycles>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((recv_handler_cycles>>24)&0xFF));

  //Reset the performance counters
  ctxt->thread_cycles = 0;
  ctxt->blocked_cycles = 0;
  ctxt->idle_cycles = 0;
  ctxt->perfmon_cycles = 0;
  ctxt->send_cycles = 0;
  ctxt->send_handler_cycles = 0;
  ctxt->recv_cycles = 0;
  ctxt->recv_handler_cycles = 0;

}
#endif

//extern "C" void softswitch_handler_exit(int code)
//{
//  uint32_t prefix=tinselId()<<8;
//
//  const PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
// 
//  tinsel_UartTryPut(prefix | 0xFF); // Magic value for key value pair
//  
//  uint32_t key=uint32_t(code);
//  tinsel_UartTryPut(prefix | ((key>>0)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>8)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>16)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>24)&0xFF));
//}


extern "C" void softswitch_handler_exit(int code)
{
  //Each message uses 1 flit
  tinsel_mboxSetLen(0);

  //get host id
  int host = tinselHostId();

  //prepare the message 
  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(0);
  msg->id = tinselId();  

  //Add the payload
  uint32_t key=uint32_t(code);
  msg->size = 5;
  msg->payload[0] = 0xFF; //Magic value for exit 
  msg->payload[1] = ((key>>0)&0xFF); 
  msg->payload[2] = ((key>>8)&0xFF); 
  msg->payload[3] = ((key>>16)&0xFF);
  msg->payload[4] = ((key>>24)&0xFF);

  //send the message    
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(host, msg); 
  
  return;
}

//extern "C" void softswitch_handler_export_key_value(uint32_t key, uint32_t value)
//{
//  uint32_t prefix=tinselId()<<8;
//
//  const PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
//  const DeviceContext *dev=ctxt->devices+ctxt->currentDevice;
//
//  tinsel_UartTryPut(prefix | 0x10); // Magic value for key value pair
//  const char *tmp=dev->id;
//  while(1){
//    tinsel_UartTryPut(prefix | *tmp);
//    if(*tmp==0)
//      break;
//    tmp++;
//  }
//  tinsel_UartTryPut(prefix | ((key>>0)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>8)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>16)&0xFF));
//  tinsel_UartTryPut(prefix | ((key>>24)&0xFF));
//  tinsel_UartTryPut(prefix | ((value>>0)&0xFF));
//  tinsel_UartTryPut(prefix | ((value>>8)&0xFF));
//  tinsel_UartTryPut(prefix | ((value>>16)&0xFF));
//  tinsel_UartTryPut(prefix | ((value>>24)&0xFF));
//}

extern "C" void softswitch_handler_export_key_value(uint32_t key, uint32_t value)
{
  //Each message uses 1 flit
  tinsel_mboxSetLen(0);

  //get host id
  int host = tinselHostId();

  //prepare the message 
  volatile hostMsg *msg = (volatile hostMsg*)tinselSlot(0);
  msg->id = tinselId();  

  //Add the payload
  msg->size = 8;
  msg->payload[0] = 0x10; //magic value keyvalue export 
  msg->payload[1] = ((key>>0)&0xFF); 
  msg->payload[2] = ((key>>8)&0xFF); 
  msg->payload[3] = ((key>>16)&0xFF);
  msg->payload[4] = ((key>>24)&0xFF);
  msg->payload[5] = ((value>>0)&0xFF);
  msg->payload[6] = ((value>>8)&0xFF);
  msg->payload[7] = ((value>>16)&0xFF);

  //send the message    
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(host, msg); 

  msg->size = 1;
  msg->payload[0] = ((value>>16)&0xFF);

  //send the message    
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(host, msg); 

  return;
}

extern "C" void __assert_func (const char *file, int line, const char *assertFunc,const char *cond)
{
  uint32_t prefix=tinselId()<<8;

  #ifndef SOFTSWITCH_MINIMAL_ASSERT
  tinsel_UartTryPut(prefix | 0xFD); // Code for an assert with info
  while(1){
    char ch=*file++;
    tinsel_UartTryPut(prefix | uint8_t(ch));
    if(ch==0){
      break;
    }
  }
  tinsel_UartTryPut(prefix | ((line>>0)&0xFF));
  tinsel_UartTryPut(prefix | ((line>>8)&0xFF));
  tinsel_UartTryPut(prefix | ((line>>16)&0xFF));
  tinsel_UartTryPut(prefix | ((line>>24)&0xFF));
  #else
  tinsel_UartTryPut(prefix | 0xFE); // Code for an assert
  #endif
  tinsel_mboxWaitUntil((tinsel_WakeupCond)0);
  while(1);
}

int main()
{
  softswitch_main();
}
