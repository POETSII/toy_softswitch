#include "tinsel.h"
#include "softswitch.hpp"
#include "tinsel_api_shim.hpp"

#include <stddef.h>
#include <cstdarg>

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


extern "C" int vsnprintf_unsigned( char * buffer, int bufsz, char pad, int width, unsigned val)
{
  // TODO: width
  // TODO: zeroPad
  
  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");
  
  static const unsigned pow10[9]={
           1,     10,      100,     1000,
       10000, 100000,  1000000, 10000000,
   100000000
  };
  
  char tmp[16]={0};
  int len=0;
  
  bool nonZero=false;
  for(int p=sizeof(pow10)/sizeof(pow10[0]); p>=0; p--){
    int d=0;
    while( val > pow10[p] ){
      val-=pow10[p];
    }
    if(d>0 || nonZero){
      tmp[len++]=d+'0';
    }
  }
  if(!nonZero){
    tmp[len++]='0';
  }
  
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
        break;
      case 'd':
        delta=vsnprintf_signed(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,signed));
        break;
      case 'x':
        delta=vsnprintf_hex(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,unsigned));
        break;
      case 's':
        delta=vsnprintf_string(buffer, (bufferMax-buffer)+1, padChar, width, va_arg(vlist,const char *));
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

  Assertion (with no further information):
  IIIIIIFE  // Magic number for assertion

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


*/

// Print a string back via stdout channel (i.e. hostlink)
extern "C" void tinsel_puts(const char *msg){
  uint32_t prefix=tinselId()<<8;
  tinselHostPut(prefix | 1);
  while(1){
    tinselHostPut(prefix | uint32_t(uint8_t(*msg)));
    if(!*msg){
      break;
    }
    msg++;
  }
}

extern "C" void softswitch_handler_log_key_value(uint32_t key, uint32_t value)
{
  return;
  
  uint32_t prefix=tinselId()<<8;

  PThreadContext *ctxt=softswitch_pthread_contexts + tinsel_myId();
  DeviceContext *dev=ctxt->devices+ctxt->currentDevice;

  tinselHostPut(prefix | 0x10); // Magic value for key value pair
  const char *tmp=dev->id;
  while(1){
    tinselHostPut(prefix | *tmp);
    if(*tmp==0)
      break;
    tmp++;
  }
  tinselHostPut(prefix | ((key>>0)&0xFF));
  tinselHostPut(prefix | ((key>>8)&0xFF));
  tinselHostPut(prefix | ((key>>16)&0xFF));
  tinselHostPut(prefix | ((key>>24)&0xFF));
  tinselHostPut(prefix | ((value>>0)&0xFF));
  tinselHostPut(prefix | ((value>>8)&0xFF));
  tinselHostPut(prefix | ((value>>16)&0xFF));
  tinselHostPut(prefix | ((value>>24)&0xFF));
}

extern "C" void __assert_func (const char *file, int line, const char *assertFunc,const char *cond)
{
  uint32_t prefix=tinselId()<<8;

  tinselHostPut(prefix | 0xFE); // Code for an assert
  tinsel_mboxWaitUntil((tinsel_WakeupCond)0);
  while(1);
}

int main()
{
  softswitch_main();
}
