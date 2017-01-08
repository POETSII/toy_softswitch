
#include <stdio.h>
#include "tinsel_api.hpp"

const unsigned MBSIZE = 128;
const unsigned SLOTS  = 4;
typedef unsigned char byte;
static byte MB[SLOTS][MBSIZE] = {"Message0","Message1","Message2","Message3"};

//------------------------------------------------------------------------------

void tinsel_flush()
// Cache flush   NOT LINKED
{
printf("tinsel_flush\n");
}

//------------------------------------------------------------------------------

void tinsel_mboxAlloc(volatile void *)
{
printf("tinsel_mboxAlloc\n");
}

//------------------------------------------------------------------------------

uint32_t tinsel_mboxCanRecv()
{
printf("tinsel_mboxCanRecv\n");
return 0;
}

//------------------------------------------------------------------------------

uint32_t tinsel_mboxCanSend()
{
printf("tinsel_mboxCanSend\n");
return 1;
}

//------------------------------------------------------------------------------

volatile void * tinsel_mboxRecv()
{
printf("tinsel_mboxRecv\n");
return 0;
}

//------------------------------------------------------------------------------

void tinsel_mboxSetLen(uint32_t n)
// Set message length for send operation          NOT LINKED
// (A message of length n is comprised of n+1 flits)
{
printf("tinsel_mboxSetLen\n");
}

//------------------------------------------------------------------------------

void tinsel_mboxSend(unsigned int, volatile void *)
{
printf("tinsel_mboxSend\n");
}

//------------------------------------------------------------------------------

volatile void * tinsel_mboxSlot(uint32_t adr)
{
for(unsigned i=0;i<SLOTS;i++) printf("Mailbox %d contains %s\n",i,MB[i]);

printf("tinsel_mboxSlot handing out access to slot %d\n",adr);
return (void *)(&MB[adr][0]);
}

//------------------------------------------------------------------------------

void tinsel_mboxWaitUntil(tinsel_WakeupCond)
// Suspend thread until wakeup condition satisfied
{
printf("tinsel_mboxWaitUntil\n");
}

//------------------------------------------------------------------------------

uint32_t tinsel_myId()
// Return a globally unique id for the calling thread        NOT LINKED
{
printf("tinsel_myId\n");
return 999;
}

//------------------------------------------------------------------------------

void tinsel_writeInstr(uint32_t addr, uint32_t word)
// Write 32-bit word to instruction memory    NOT LINKED
{
printf("tinsel_writeInstr\n");
}


unsigned tinsel_mboxSlotCount()
{
    printf("tinsel_mboxSlotCount\n");
    return 4;
}

//------------------------------------------------------------------------------







