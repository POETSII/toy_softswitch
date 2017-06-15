#ifndef tinsel_api_shim_hpp
#define tinsel_api_shim_hpp

#include <cstdint>
#include <cassert>

#include "tinsel_api.hpp"

#include "config.h"

// Return a globally unique id for the calling thread
uint32_t tinsel_myId()
{
    return tinselId();
}

// Write 32-bit word to instruction memory
inline void tinsel_writeInstr(uint32_t addr, uint32_t word)
{
    tinselWriteInstr(addr,word);
}

// Cache flush
void tinsel_flush()
{
    tinselCacheFlush();
}

// Get pointer to nth message-aligned slot in mailbox scratchpad
volatile void* tinsel_mboxSlot(uint32_t n)
{
    return tinselSlot(n);
}

// Determine if calling thread can send a message
uint32_t tinsel_mboxCanSend()
{
    return tinselCanSend();
}

// Set message length for send operation
// Message length in bytes
void tinsel_mboxSetLen(uint32_t n)
{
  uint32_t flitCount=(n+TinselWordsPerFlit+1) & (TinselWordsPerFlit-1);
  tinselSetLen(flitCount);
}

// Send message at address to destination
// (Address must be aligned on message boundary)
void tinsel_mboxSend(uint32_t dest, volatile void* addr)
{
    tinselSend(dest,addr);
}

// Give mailbox permission to use given address to store an incoming message
void tinsel_mboxAlloc(volatile void* addr)
{
    tinselAlloc(addr);
}

// Determine if calling thread can receive a message
uint32_t tinsel_mboxCanRecv()
{
    return tinselCanRecv();
}

// Receive message
volatile void* tinsel_mboxRecv()
{
    return tinselRecv();
}

// Suspend thread until wakeup condition satisfied
void tinsel_mboxWaitUntil(tinsel_WakeupCond cond)
{
    tinselWaitUntil((TinselWakeupCond)cond);
}

// Get the number of slots. Should this come from config.h?
unsigned tinsel_mboxSlotCount()
{
    return 1<<TinselLogMsgsPerThread;
}

inline void tinsel_hostPut(uint32_t x);




#endif
