

#include <cstdint>
#include <cassert>

// Return a globally unique id for the calling thread
uint32_t T_myId();

// Write 32-bit word to instruction memory
void T_writeInstr(uint32_t addr, uint32_t word);

// Cache flush
void T_flush();

// Get pointer to nth message-aligned slot in mailbox scratchpad
volatile void* T_mboxSlot(uint32_t n);

// Determine if calling thread can send a message
uint32_t T_mboxCanSend();

// Set message length for send operation
// (A message of length n is comprised of n+1 flits)
void T_mboxSetLen(uint32_t n);

// Send message at address to destination
// (Address must be aligned on message boundary)
void T_mboxSend(uint32_t dest, volatile void* addr);

// Give mailbox permission to use given address to store an incoming message
void T_mboxAlloc(volatile void* addr);

// Determine if calling thread can receive a message
uint32_t T_mboxCanRecv();

// Receive message
volatile void* T_mboxRecv();

// Thread can be woken by a logical-OR of these events
typedef enum {T_CAN_SEND = 1, T_CAN_RECV = 2} T_WakeupCond;

// Suspend thread until wakeup condition satisfied
void T_mboxWaitUntil(T_WakeupCond cond);
