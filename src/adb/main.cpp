
#include "tinsel_api.hpp"
#include "softswitch.hpp"

#include <stdio.h>

void                    f_idle         (const void *,const void *,void *);
void                    f_recv         (const void *,const void *,void *,
                                        const void *,void *,const void *);
uint32_t                f_rts          (const void *,const void *,void *);
bool                    f_send         (const void *,const void *,void *,
                                        void *);

void                    setup          (PThreadContext *);
extern "C" void         softswitch_main();


//------------------------------------------------------------------------------

// New method of linking by symbol
PThreadContext softswitch_pthread_contexts[1];
unsigned softswitch_pthread_count = 1;

int main(int argc, char* argv[])
{
printf("Hello, world\n");

PThreadContext &stuff = softswitch_pthread_contexts[0];
setup(&stuff);

// Implicitly passed by softswitch_pthread_contexts
softswitch_main();

printf("Hit any key to go...\n");
getchar();
}

//------------------------------------------------------------------------------

void f_idle(const void * graphProps,const void * devProps,void * devState)
{
printf("Dummy f_idle()\n");
}

//------------------------------------------------------------------------------

void f_recv(const void * graphProps,const void * devProps,void * devState,
            const void * edgeProps,void * edgeState,const void *)
{
printf("Dummy f_recv()\n");
}

//------------------------------------------------------------------------------

uint32_t f_rts(const void * graphProps,const void * devProps,void * devState)
{
printf("Dummy f_rts\n");
return true;

}

//------------------------------------------------------------------------------

bool f_send(const void * graphProps,const void * devProps,
            void * devState,void * message)
{
printf("Dummy f_send\n");
return true;
}

//------------------------------------------------------------------------------

void setup(PThreadContext * pTC)
{
printf("Setting up thread context\n");
pTC->graphProps = 0;
pTC->numVTables = 1;
pTC->vtables    = new DeviceTypeVTable;
pTC->numDevices = 1;
pTC->devices    = new DeviceContext;
pTC->rtsHead    = 0;
pTC->rtsTail    = 0;
pTC->rtcChecked = 0;
pTC->rtcOffset  = 0;

pTC->vtables->numInputs          = 1;
pTC->vtables->inputPorts         = new InputPortVTable;

pTC->vtables->numOutputs         = 1;
pTC->vtables->outputPorts        = new OutputPortVTable;

pTC->vtables->readyToSendHandler = f_rts;
printf("f_rts  = %p\n",f_rts);
pTC->vtables->computeHandler     = f_idle;
printf("f_idle = %p\n",f_idle);

pTC->devices->vtable             = pTC->vtables;
pTC->devices->properties         = 0;
pTC->devices->state              = 0;
pTC->devices->index              = 0;
pTC->devices->targets            = new OutputPortTargets;
pTC->devices->rtsFlags           = 0x0;
pTC->devices->rtc                = false;
pTC->devices->prev               = 0;
pTC->devices->next               = 0;

pTC->devices->targets->numTargets = 1;
pTC->devices->targets->targets = new address_t;
pTC->devices->targets->targets->thread = 99;
pTC->devices->targets->targets->device = 88;
pTC->devices->targets->targets->port   = 77;
pTC->devices->targets->targets->flag   = 66;

pTC->vtables->inputPorts->receiveHandler = f_recv;
pTC->vtables->outputPorts->sendHandler   = f_send;

}

//------------------------------------------------------------------------------

