#include "tinsel_api.hpp"

#include "softswitch.hpp"

#include <cstdio>

//! Initialise data-structures (e.g. RTS)
extern "C" void softswitch_init(PThreadContext *ctxt);

//! Do some idle work. Return false if there is nothing more to do
extern "C" bool softswitch_onIdle(PThreadContext *ctxt);

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message);

//! Prepare a message on the given buffer
/*! \param numTargets Receives the number of addresses to send the packet to
    \param pTargets If numTargets>0, this will be pointed at an array with numTargets elements
    \retval Size of message in bytes
*/
extern "C" unsigned softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets);


extern "C" void softswitch_main()
{
    PThreadContext *ctxt=&softswitch_pthread_context;
    
    fprintf(stderr, "softswitch: init\n");
    softswitch_init(ctxt);
    
    
    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSlot(0);

    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;
    uint32_t currSize=0;

    // Assumption: all buffers are owned by software, so we have to give them to mailbox
    // We only keep hold of slot 0
    fprintf(stderr, "softswitch: giving receive buffers to mailbox\n");
    for(unsigned i=1; i<tinsel_mboxSlotCount(); i++){
        tinsel_mboxAlloc( tinsel_mboxSlot(i) );
    }

    fprintf(stderr, "softswitch: starting loop\n");
    
    while(1) {
        fprintf(stderr, "softswitch: loop top\n");
        
        // Only want to send if either:
        // - we are currently sending message,
        // - or at least one device wants to send one
        bool wantToSend = (currSendTodo>0) || softswitch_IsRTSReady(ctxt);
        fprintf(stderr, "softswitch:  wantToSend=%d\n", wantToSend?1:0);


        // Run idle if:
        // - There is nothing to receive
        // - we aren't able to send or we don't want to send
        while( (!tinsel_mboxCanRecv()) && (!wantToSend || !tinsel_mboxCanSend()) ){
            if(!softswitch_onIdle(ctxt))
                break;
        }

        uint32_t wakeupFlags = wantToSend ? (TINSEL_CAN_RECV|TINSEL_CAN_SEND) : TINSEL_CAN_RECV;
        fprintf(stderr, "softswitch: waiting for send=%d, recv=%d\n", wakeupFlags&TINSEL_CAN_SEND, (wakeupFlags&TINSEL_CAN_RECV)?1:0);
        tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );

        if(tinsel_mboxCanRecv()){
            fprintf(stderr, "softswitch:   receive branch\n");

            /*! We always receive messages, even while a send is in progress (currSendTodo > 0) */

            fprintf(stderr, "softswitch:   calling mboxRecv\n");
            auto recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool

            fprintf(stderr, "softswitch:   calling onRecieve, recvBuffer=%p\n", recvBuffer);
            softswitch_onReceive(ctxt, (const void *)recvBuffer);  // Decode and dispatch

            fprintf(stderr, "softswitch:   giving buffer back\n");
            tinsel_mboxAlloc(recvBuffer); // Put it back in receive pool
        }else{
            fprintf(stderr, "softswitch:   send branch\n");

            assert(wantToSend); // Only come here if we have something to do
            assert(tinsel_mboxCanSend()); // Only reason we could have got here

            /* Either we have to finish sending a previous message to more
               addresses, or we get the chance to send a new message. */

            if(currSendTodo==0){
                fprintf(stderr, "softswitch:   preparing new message\n");

                // Prepare a new packet to send
                currSize=softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList);
            }else{
                // We still have more addresses to deliver the last message to
                fprintf(stderr, "softswitch:   forwarding current message\n");
            }

            fprintf(stderr, "softswitch:   sending to thread %08x, device %u, port %u\n", currSendAddressList->thread, currSendAddressList->device, currSendAddressList->port);
            
            // Update the target address (including the device and pin)
            ((packet_t*)sendBuffer)->dest = *currSendAddressList;

            // Send to the relevant thread
            // TODO: Shouldn't there be something like mboxForward as part of
            // the API, which only takes the address?
            fprintf(stderr, "softswitch:   setting length\n");
            tinsel_mboxSetLen(currSize);
            fprintf(stderr, "softswitch:   doing send\n");
            tinsel_mboxSend(currSendAddressList->thread, sendBuffer);

            // Move onto next address for next time
            currSendTodo--; // If this reaches zero, we are done with the message
            currSendAddressList++;
        }
        
        fprintf(stderr, "softswitch: loop bottom\n");
    }
}
