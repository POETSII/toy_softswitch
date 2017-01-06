#include "tinsel_api.hpp"

#include "softswitch.hpp"

//! Initialise data-structures (e.g. RTS)
extern "C" void softswitch_init(PThreadContext *ctxt);

//! Do some idle work. Return false if there is nothing more to do
extern "C" bool softswitch_onIdle(PThreadContext *ctxt);

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt,const void *message);

//! Prepare a message on the given buffer
/*! \param numTargets Receives the number of addresses to send the packet to
    \param pTargets If numTargets>0, this will be pointed at an array with numTargets elements
*/
extern "C" void softswitch_onSend(PThreadContext *ctxt, void *message, uint32_t &numTargets, const address_t *&pTargets);


void softswitch_main(PThreadContext *ctxt)
{
    softswitch_init(ctxt);

    // We'll hold onto this one
    volatile void *sendBuffer=tinsel_mboxSlot(0);

    // If a send is in progress, this will be non-null
    const address_t *currSendAddressList=0;
    uint32_t currSendTodo=0;

    // Assumption: all buffers are owned by mailbox
    // until we use tinsel_mboxSlot to get hold of them (?)

    while(1) {
        // Only want to send if either:
        // - we are currently sending message,
        // - or at least one device wants to send one
        bool wantToSend = (currSendTodo>0) || softswitch_IsRTSReady(ctxt);

        // Run idle if:
        // - There is nothing to receive
        // - we aren't able to send or we don't want to send
        while( (!tinsel_mboxCanRecv()) && (!wantToSend || !tinsel_mboxCanSend()) ){
            if(!softswitch_onIdle(ctxt))
                break;
        }

        uint32_t wakeupFlags = wantToSend ? (TINSEL_CAN_RECV|TINSEL_CAN_SEND) : TINSEL_CAN_RECV;
        tinsel_mboxWaitUntil( (tinsel_WakeupCond) wakeupFlags );

        if(tinsel_mboxCanRecv()){
            /*! We always receive messages, even while a send is in progress (currSendTodo > 0) */

            auto recvBuffer=tinsel_mboxRecv();     // Take the buffer from receive pool

            softswitch_onReceive(ctxt, (const void *)recvBuffer);  // Decode and dispatch

            tinsel_mboxAlloc(recvBuffer); // Put it back in receive pool
        }else{
            assert(wantToSend); // Only come here if we have something to do
            assert(tinsel_mboxCanSend()); // Only reason we could have got here

            /* Either we have to finish sending a previous message to more
               addresses, or we get the chance to send a new message. */

            if(currSendTodo==0){
                // Prepare a new packet to send
                softswitch_onSend(ctxt, (void*)sendBuffer, currSendTodo, currSendAddressList);
            }else{
                // We still have more addresses to deliver the last message to
            }

            // Update the target address (including the device and pin)
            ((packetinsel_t*)sendBuffer)->dest = *currSendAddressList;

            // Send to the relevant thread
            tinsel_mboxSend(currSendAddressList->thread, sendBuffer);

            // Move onto next address for next time
            currSendTodo--; // If this reaches zero, we are done with the message
            currSendAddressList++;
        }
    }
}
