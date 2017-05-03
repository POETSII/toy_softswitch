#include "tinsel.h"
#include "softswitch.hpp"
#include "tinsel_api_shim.hpp"

#include <stddef.h>
#include <cstdarg>

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
  
  int len=strlen(data);
  while(bufsz>1 && width>len){
    *buffer++=pad;
    bufsz--;
    width--;
    done++;
  }
  
  while(bufsz>1 && len>0){
    *buffer++ = *data++;
    bufsz--;
    done++;
  }
  return done;
}



extern "C" int vsnprintf_hex( char * buffer, int bufsz, char pad, int width, unsigned val)
{
  
  static_assert(sizeof(unsigned)==4,"Assuming we have 32-bit integers...");
  
  char tmp[16]={0};
  int len=0;
  
  bool nonZero=false;
  for(int p=7;p>=0;p--){
    int d=val>>28;
    if(d>0 || nonZero){
      tmp[len++]=d;
    }
    val=val<<4;
  }
  if(!nonZero){
    tmp[len++]='0';
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

extern "C" int isdigit(int ch)
{
  return '0'<=ch && ch <='9';
}

extern "C" int vsnprintf( char * buffer, int bufsz, const char * format, va_list vlist )
{
  memset(buffer, 0, bufsz);
  
  int done=0;
  
  // Must leave space for null, so bufsz>1
  while(bufsz>1 && *format){
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
      case '%':
        *buffer=ch;
        delta=1;
        break;
      case 'u':
        delta=vsnprintf_unsigned(buffer, bufsz-done, padChar, width, va_arg(vlist,unsigned));
        break;
      case 'x':
        delta=vsnprintf_hex(buffer, bufsz-done, padChar, width, va_arg(vlist,unsigned));
        break;
      case 's':
        delta=vsnprintf_string(buffer, bufsz-done, padChar, width, va_arg(vlist,const char *));
        break;
      default:
        delta=vsnprintf_string(buffer, bufsz-done, padChar, width, "<ERROR-UnknownPrintfString>");
        break;
      }
    }else{
      *buffer=ch;
      delta=1;
    }
    done+=delta;
    
  }
  return done;
}

#endif


/* Protocol: 

  We need to send data back up to the host, but each word needs to
  include the source (I think). So we'll use a 16-bit thread id.

  Each word consists of 16-bits of id (hi) and 16-bits of payload (lo)

  stdout:
  IIIIII01  // 1 = stdin
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

  Pair of 32-bit values
  IIIIII10  // Magic number
  IIIIIIKK  // 8-bits of key (LSB)
  IIIIIIKK  // 8-bits of key
  IIIIIIKK  // 8-bits of key
  IIIIIIKK  // 8-bits of key (MSB)
  IIIIIIVV  // 8-bits of key (LSB)
  IIIIIIVV  // 8-bits of key
  IIIIIIVV  // 8-bits of key
  IIIIIIVV  // 8-bits of key (MSB)


*/

// Print a string back via debugging channel (i.e. hostlink)
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

void tinsel_puts_abc(){
  uint32_t prefix=tinselId()<<8;
  tinselHostPut(prefix | 1);
  tinselHostPut(prefix | 'A');
  tinselHostPut(prefix | 'B');
  tinselHostPut(prefix | 'C');
  tinselHostPut(prefix | '\n');
  tinselHostPut(prefix | 0);
}


extern "C" void softswitch_handler_log_key_value(uint32_t key, uint32_t value)
{
  uint32_t prefix=tinselId()<<8;

  tinselHostPut(prefix | 0x10); // Magic value for key value pair
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
