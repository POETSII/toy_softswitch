#ifndef tinsel_api_hpp
#define tinsel_api_hpp

#include <cstdint>
#include <cassert>

// Return a globally unique id for the calling thread
uint32_t tinsel_myId();

// Write 32-bit word to instruction memory
void tinsel_writeInstr(uint32_t addr, uint32_t word);

// Cache flush
void tinsel_flush();

// Get pointer to nth message-aligned slot in mailbox scratchpad
volatile void* tinsel_mboxSlot(uint32_t n);

// Determine if calling thread can send a message
uint32_t tinsel_mboxCanSend();

// Set message length for send operation
// (A message of length n is comprised of n+1 flits)
void tinsel_mboxSetLen(uint32_t n);

// Send message at address to destination
// (Address must be aligned on message boundary)
void tinsel_mboxSend(uint32_t dest, volatile void* addr);

// Give mailbox permission to use given address to store an incoming message
void tinsel_mboxAlloc(volatile void* addr);

// Determine if calling thread can receive a message
uint32_t tinsel_mboxCanRecv();

// Receive message
volatile void* tinsel_mboxRecv();

// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} T_WakeupCond;

// Suspend thread until wakeup condition satisfied
void tinsel_mboxWaitUntil(T_WakeupCond cond);

#endif
