#ifndef tinsel_api_hpp
#define tinsel_api_hpp

#include <cstdint>
#include <cassert>

// Thread can be woken by a logical-OR of these events
typedef enum {tinsel_CAN_SEND = 1, tinsel_CAN_RECV = 2} tinsel_WakeupCond;


#include "tinsel_api_shim.hpp"

// Return a globally unique id for the calling thread
inline uint32_t tinsel_myId();

// Write 32-bit word to instruction memory
inline void tinsel_writeInstr(uint32_t addr, uint32_t word);

// Cache flush
inline void tinsel_flush();

// Get pointer to nth message-aligned slot in mailbox scratchpad
inline volatile void* tinsel_mboxSlot(uint32_t n);

// Determine if calling thread can send a message
inline uint32_t tinsel_mboxCanSend();

// Set message length for send operation
// Message length in bytes
inline void tinsel_mboxSetLen(uint32_t n);

// Send message at address to destination
// (Address must be aligned on message boundary)
inline void tinsel_mboxSend(uint32_t dest, volatile void* addr);

// Give mailbox permission to use given address to store an incoming message
inline void tinsel_mboxAlloc(volatile void* addr);

// Determine if calling thread can receive a message
inline uint32_t tinsel_mboxCanRecv();

// Receive message
inline volatile void* tinsel_mboxRecv();

// Suspend thread until wakeup condition satisfied
inline void tinsel_mboxWaitUntil(tinsel_WakeupCond cond);

// Get the number of slots. Should this come from config.h?
inline unsigned tinsel_mboxSlotCount();

// Print a string back via debugging channel (i.e. hostlink)
extern "C" void tinsel_puts(const char *msg);

extern "C" void softswitch_main();



#endif
