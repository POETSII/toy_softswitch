#include "tinsel_mbox.hpp"

const unsigned LogMsgsPerThread = 4;
const unsigned LogWordsPerFlit = 2;
const unsigned LogMaxFlitsPerMsg = 4;
    
typedef TinselMbox<LogMsgsPerThread,LogWordsPerFlit,LogMaxFlitsPerMsg> mbox_t;

static mbox_t *mbox = 0;

#include <mpi/mpi.h>

#include <vector>
#include <sstream>

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


void tinsel_puts(const char *msg)
{
    fputs(msg, stderr);
}


extern "C" void softswitch_handler_exit(int code)
{
    fprintf(stderr, "Handler exit : code=%d\n", code);
    exit(code);
}


void mbox_thread(uint32_t threadId, int hardLogLevel)
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
            if(hardLogLevel >= 1){
                fprintf(stderr, "[%u] HARD / in : Starting thread\n", threadId);     
            }
            
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
                
                uint32_t srcThreadId;
                srcThreadId=((packet_t*)buffer)->source.thread;
                if(hardLogLevel >= 2){
                    fprintf(stderr, "[%u] HARD / in : Received message of length %u from rank '%u' (srcThread=0x%x)\n", threadId, len, status.MPI_SOURCE, srcThreadId);     
                }
                assert((int)srcThreadId == status.MPI_SOURCE);
               
                uint32_t dstThreadId=((packet_t*)buffer)->dest.thread;

                if(hardLogLevel >=2 ){
                    fprintf(stderr, "[%u] HARD / in : delivering message of length %u from thread 0x%x to thread 0x%x\n", threadId, len, srcThreadId, dstThreadId);                 
                }
                mboxLocal->netPushMessage(dstThreadId, len, buffer);
            }           
        }catch(std::exception &e){
            fprintf(stderr, "[%u] HARD / in : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });
    
    
    std::thread outgoing([&](){
        try{
            if(hardLogLevel >=1){
                fprintf(stderr, "[%u] HARD / out : Starting outgoing thread\n", threadId);     
            }
            
            unsigned bufferLen=mbox_t::WordsPerMsg*4;
            void *buffer=malloc(bufferLen);
            
            while(1){
                uint32_t dstThreadId;
                uint32_t byteLength;
                
                if(hardLogLevel >= 2){
                    fprintf(stderr, "[%u] HARD / out : Waiting to pull message from mailbox\n", threadId);     
                }
                mboxLocal->netPullMessage(dstThreadId, byteLength, buffer);
                
                assert(byteLength <= 4*mbox_t::WordsPerMsg);
                
                if(hardLogLevel >= 2){
                    fprintf(stderr, "[%u] HARD / out :  Pulled message to %x of length %u\n", threadId, dstThreadId, byteLength);     
                }
                
                
                if(MPI_SUCCESS != MPI_Send(buffer, byteLength, MPI_BYTE, dstThreadId, /*tag*/0, MPI_COMM_WORLD)){
                    throw std::runtime_error("MPI_Send");
                }
                
                if(hardLogLevel >= 2){
                    fprintf(stderr, "[%u] HARD / out : Send message to %x\n", threadId, dstThreadId);     
                }

            }
        }catch(std::exception &e){
            fprintf(stderr, "[%u] HARD / out : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });
   
    if(hardLogLevel >= 1){
        fprintf(stderr, "[%u] HARD : Starting soft-switch\n", threadId);     
    }
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
            std::stringstream acc;
            acc<<"MPI_Init_thread, level too low: wanted "<<MPI_THREAD_MULTIPLE<<", but got "<<providedLevel;
            throw std::runtime_error(acc.str());
        }
        
        int mpiSize;
        MPI_Comm_size(MPI_COMM_WORLD,&mpiSize);

        int mpiRank;
        MPI_Comm_rank(MPI_COMM_WORLD,&mpiRank);

        if(mpiSize < softswitch_pthread_count){
            if(mpiRank==0){
                fprintf(stderr, "Error : MPI world size of %u too small. Need at least %u for this application\n", mpiSize, softswitch_pthread_count);
            }
            MPI_Finalize();
            exit(1);
        }        
        if(mpiSize > softswitch_pthread_count){
            if(mpiRank==0){
                fprintf(stderr, "Warning : MPI world size too large. Only need %u for this application\n", softswitch_pthread_count);
            }
        }
        
        int applLogLevel=4;
        int softLogLevel=4;
        int hardLogLevel=4;
        
        for(int i=0; i<softswitch_pthread_count; i++){
            softswitch_pthread_contexts[i].applLogLevel=applLogLevel;
            softswitch_pthread_contexts[i].softLogLevel=softLogLevel;
            softswitch_pthread_contexts[i].hardLogLevel=hardLogLevel;
        }
        
        mbox_thread(mpiRank, hardLogLevel);
        
        MPI_Finalize();
    }catch(std::exception &e){
        fprintf(stderr, "Exception: %s\n", e.what());
        MPI_Finalize();
        throw;
    }
    
    return 0;
}
