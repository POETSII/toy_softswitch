#include "tinsel_mbox.hpp"

const unsigned LogMsgsPerThread = 4;
const unsigned LogWordsPerFlit = 2;
const unsigned LogMaxFlitsPerMsg = 4;
    
typedef TinselMbox<LogMsgsPerThread,LogWordsPerFlit,LogMaxFlitsPerMsg> mbox_t;

static mbox_t *mbox = 0;

#include <mpi/mpi.h>

#include <vector>

#include "softswitch.hpp"


uint32_t tinsel_myId()
{ return mbox->getThreadId(); }

volatile void* tinsel_mboxSlot(uint32_t n)
{ return mbox->mboxSlot(n); }

uint32_t tinsel_mboxCanSend()
{ return mbox->mboxCanSend(); }

void tinsel_mboxSetLen(uint32_t n)
{ mbox->mboxSetLen(n); }

void tinsel_mboxSend(uint32_t dest, volatile void* addr)
{
    // We don't need volatile, as we operate under mutex which should cause fence
    mbox->mboxSend(dest, (void*)addr);
}

// Give mailbox permission to use given address to store an incoming message
void tinsel_mboxAlloc(volatile void* addr)
{
    // We don't need volatile, as we operate under mutex which should cause fence
    mbox->mboxAlloc((void*)addr);
}

// Determine if calling thread can receive a message
uint32_t tinsel_mboxCanRecv()
{ return mbox->mboxCanRecv(); }

// Receive message
volatile void* tinsel_mboxRecv()
{ return mbox->mboxRecv(); }

// Suspend thread until wakeup condition satisfied
void tinsel_mboxWaitUntil(tinsel_WakeupCond cond)
{ mbox->mboxWaitUntil(cond); }

unsigned tinsel_mboxSlotCount()
{ return mbox_t::MsgsPerThread; }


void mbox_thread(uint32_t threadId)
{
    mbox=new mbox_t();
    
    // Take local copy so that our worker threads don't
    // end up on a different variable.
    mbox_t *mboxLocal=mbox;
    
    mbox->init(threadId);
       
    // Ok, we have our unique socket, so now we need to:
    // - Listen for incoming messages and push them into mailbox
    // - pop messages from the mailbox and send to the socket
    // - Run the softswitch thread
 
    std::thread incoming([&](){
        try{        
            fprintf(stderr, "Thread/0x%08x/in : Starting incoming thread\n", threadId);     
            
            unsigned bufferLen=mbox_t::WordsPerMsg*4;
            void *buffer=malloc(bufferLen);
            
            while(1){
                MPI_Status status;
                if(MPI_SUCCESS!=MPI_Recv(buffer, bufferLen, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG,
                    MPI_COMM_WORLD, &status)){
                    throw std::runtime_error("MPI_Recv error");
                }
                
                int len;
                if(MPI_SUCCESS!=MPI_Get_count(&status, MPI_BYTE, &len)){
                    throw std::runtime_error("MPI_Get_count");
                }
                
                fprintf(stderr, "Thread/0x%08x/in : Received message of length %u from rank '%u'\n", threadId, len, status.MPI_SOURCE);     
                
                uint32_t srcThreadId;
                srcThreadId=((packet_t*)buffer)->source.thread;
                assert((int)srcThreadId == status.MPI_SOURCE);
               
                uint32_t dstThreadId=((packet_t*)buffer)->dest.thread;

                fprintf(stderr, "Thread/0x%08x/in : delivering message of length %u from thread 0x%08x to thread 0x%08x\n", threadId, len, srcThreadId, dstThreadId);                 
                mboxLocal->netPushMessage(dstThreadId, len, buffer);
            }           
        }catch(std::exception &e){
            fprintf(stderr, "Thread/0x%08x/in : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });
    
    
    std::thread outgoing([&](){
        try{
            fprintf(stderr, "Thread/0x%08x/out : Starting outgoing thread\n", threadId);     
            
            unsigned bufferLen=mbox_t::WordsPerMsg*4;
            void *buffer=malloc(bufferLen);
            
            while(1){
                uint32_t dstThreadId;
                uint32_t byteLength;
                
                fprintf(stderr, "Thread/0x%08x/out : Waiting to pull message from mailbox\n", threadId);     
                mboxLocal->netPullMessage(dstThreadId, byteLength, buffer);
                
                assert(byteLength <= 4*mbox_t::WordsPerMsg);
                
                fprintf(stderr, "Thread/0x%08x/out : Pulled message to %08x of length %u\n", threadId, dstThreadId, byteLength);     
                
                
                if(MPI_SUCCESS != MPI_Send(buffer, byteLength, MPI_BYTE, dstThreadId, /*tag*/0, MPI_COMM_WORLD)){
                    throw std::runtime_error("MPI_Send");
                }
                
                fprintf(stderr, "Thread/0x%08x/out : Send message to %08x\n", threadId, dstThreadId);     

            }
        }catch(std::exception &e){
            fprintf(stderr, "Thread/0x%08x/out : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });
   
    fprintf(stderr, "Thread/0x%08x/cpu : Starting soft-switch\n", threadId);     
    softswitch_main();

    incoming.join();
    outgoing.join();

}




int main(int argc, char *argv[])
{
    try{
        
        int providedLevel;
        if(MPI_SUCCESS != MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE , &providedLevel ) ){
            throw std::runtime_error("MPI_Init_thread");
        }
        if(providedLevel < MPI_THREAD_MULTIPLE){
            throw std::runtime_error("MPI_Init_thread, level too low.");
        }
        
        int mpiSize;
        MPI_Comm_size(MPI_COMM_WORLD,&mpiSize);

        int mpiRank;
        MPI_Comm_rank(MPI_COMM_WORLD,&mpiRank);

        if(mpiSize < softswitch_pthread_count){
            if(mpiRank==0){
                fprintf(stderr, "Error : MPI world size too small. Need at least %u for this application\n", softswitch_pthread_count);
            }
            MPI_Finalize();
            exit(1);
        }        
        if(mpiSize < softswitch_pthread_count){
            if(mpiRank==0){
                fprintf(stderr, "Warning : MPI world size too large. Only need %u for this application\n", softswitch_pthread_count);
            }
        }
        
        mbox_thread(mpiRank);
        
        MPI_Finalize();
    }catch(std::exception &e){
        fprintf(stderr, "Exception: %s\n", e.what());
        MPI_Finalize();
        throw;
    }
    
    return 0;
}
