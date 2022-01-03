#ifndef tinsel_api_shim_hpp
#define tinsel_api_shim_hpp

#include <cstdint>
#include <cassert>



#include "config.h"

#include "tinsel.h"

#define POETS_ALWAYS_INLINE inline __attribute__((always_inline))

// Return a count of the clock cycle for the current device
POETS_ALWAYS_INLINE uint32_t tinsel_CycleCount()
{
	return tinselCycleCount();
}

// Return a globally unique id for the calling thread
POETS_ALWAYS_INLINE uint32_t tinsel_myId()
{
    return tinselId();
}

// Write 32-bit word to instruction memory
POETS_ALWAYS_INLINE void tinsel_writeInstr(uint32_t addr, uint32_t word)
{
    tinselWriteInstr(addr,word);
}

// Cache flush
POETS_ALWAYS_INLINE void tinsel_flush()
{
    tinselCacheFlush();
}

/*
// Get pointer to nth message-aligned slot in mailbox scratchpad
POETS_ALWAYS_INLINE volatile void* tinsel_mboxSlot(uint32_t n)
{
    return tinselSlot(n);
}
*/

// Get pointer to thread's message slot reserved for sending
POETS_ALWAYS_INLINE volatile void* tinsel_mboxSendSlot()
{
    return tinselSendSlot();
}

POETS_ALWAYS_INLINE volatile void* tinsel_mboxSendSlotExtra()
{
    return tinselSendSlotExtra();
}

// Determine if calling thread can send a message
POETS_ALWAYS_INLINE uint32_t tinsel_mboxCanSend()
{
    return tinselCanSend();
}

// Set message length for send operation
// Message length in bytes
POETS_ALWAYS_INLINE void tinsel_mboxSetLen(uint32_t n)
{

  //assert(n <= (TinselWordsPerFlit * 4 * 4)); // 4 flits per message max, 4 bytes per word
  uint32_t flitCnt = (n-1) >> TinselLogBytesPerFlit;
  if(flitCnt < 0) {
    flitCnt = 0;
  }
  tinselSetLen(flitCnt);

}

// Send message at address to destination
// (Address must be aligned on message boundary)
POETS_ALWAYS_INLINE void tinsel_mboxSend(uint32_t dest, volatile void* addr)
{
    tinselSend(dest,addr);
}

/*
// Give mailbox permission to use given address to store an incoming message
POETS_ALWAYS_INLINE void tinsel_mboxAlloc(volatile void* addr)
{
    tinselAlloc(addr);
}
*/

// Indicate that we've finished with the given message.
POETS_ALWAYS_INLINE void tinsel_mboxFree(volatile void* addr)
{
    tinselFree(addr);
}

// Determine if calling thread can receive a message
POETS_ALWAYS_INLINE uint32_t tinsel_mboxCanRecv()
{
    return tinselCanRecv();
}

// Receive message
POETS_ALWAYS_INLINE volatile void* tinsel_mboxRecv()
{
    return tinselRecv();
}

// Suspend thread until wakeup condition satisfied
POETS_ALWAYS_INLINE void tinsel_mboxWaitUntil(tinsel_WakeupCond cond)
{
    tinselWaitUntil((TinselWakeupCond)cond);
}

POETS_ALWAYS_INLINE int tinsel_mboxIdle(bool vote)
{
    return tinselIdle(vote);
}

/*
// Get the number of slots. Should this come from config.h?
POETS_ALWAYS_INLINE unsigned tinsel_mboxSlotCount()
{
    return 1<<TinselLogMsgsPerThread;
}
*/

inline void tinsel_hostPut(uint32_t x);

// Get globally unique thread id of host
// (Host board has X coordinate of 0 and Y coordinate on mesh rim)
POETS_ALWAYS_INLINE uint32_t tinsel_HostId()
{
  return tinselHostId();
}


#endif
