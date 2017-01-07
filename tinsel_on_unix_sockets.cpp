#include "tinsel_mbox.hpp"

const unsigned LogMsgsPerThread = 4;
const unsigned LogWordsPerFlit = 2;
const unsigned LogMaxFlitsPerMsg = 4;

typedef TinselMbox<LogMsgsPerThread,LogWordsPerFlit,LogMaxFlitsPerMsg> mbox_t;

mbox_t mbox;

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "softswitch.hpp"


uint32_t tinsel_myId()
{ return mbox.getThreadId(); }

volatile void* tinsel_mboxSlot(uint32_t n)
{ return mbox.mboxSlot(n); }

uint32_t tinsel_mboxCanSend()
{ return mbox.mboxCanSend(); }

void tinsel_mboxSetLen(uint32_t n)
{ mbox.mboxSetLen(n); }

void tinsel_mboxSend(uint32_t dest, volatile void* addr)
{
    // We don't need volatile, as we operate under mutex which should cause fence
    mbox.mboxSend(dest, (void*)addr);
}

// Give mailbox permission to use given address to store an incoming message
void tinsel_mboxAlloc(volatile void* addr)
{
    // We don't need volatile, as we operate under mutex which should cause fence
    mbox.mboxAlloc((void*)addr);
}

// Determine if calling thread can receive a message
uint32_t tinsel_mboxCanRecv()
{ return mbox.mboxCanRecv(); }

// Receive message
volatile void* tinsel_mboxRecv()
{ return mbox.mboxRecv(); }

// Suspend thread until wakeup condition satisfied
void tinsel_mboxWaitUntil(tinsel_WakeupCond cond)
{ mbox.mboxWaitUntil(cond); }

unsigned tinsel_mboxSlotCount()
{ return mbox_t::MsgsPerThread; }

extern PThreadContext pthread_ctxt;


int main(int argc, const char *argv[])
{
    int threadId=atoi(argv[1]);
    const char *socketDir=argv[2];
    
    char addrTemplate[128];
    snprintf(addrTemplate, sizeof(addrTemplate), "%s/%%08xl", socketDir);
    puts(addrTemplate);
    
    mbox.init(threadId);
        
    struct sockaddr_un mboxAddr;
    memset(&mboxAddr,0,sizeof(sockaddr_un));
    mboxAddr.sun_family=AF_UNIX;
    snprintf(mboxAddr.sun_path, 107, addrTemplate, threadId);

    fprintf(stderr, "Thread/0x%08x : Opening socket %s\n", threadId, mboxAddr.sun_path); 
    int mboxSocket=socket(AF_UNIX,SOCK_DGRAM,0);
    if(mboxSocket==-1){
        perror("socket");
        exit(1);
    }
    
    if(-1==bind(mboxSocket, (struct sockaddr *)&mboxAddr,sizeof(struct sockaddr_un))){
        perror("bind");
        exit(1);
    }
    
    // Ok, we have our unique socket, so now we need to:
    // - Listen for incoming messages and push them into mailbox
    // - pop messages from the mailbox and send to the socket
    // - Run the softswitch thread
 
    std::thread incoming([&](){
        fprintf(stderr, "Thread/0x%08x/in : Starting incoming thread\n", threadId);     
        
        unsigned bufferLen=mbox_t::WordsPerMsg*4;
        void *buffer=malloc(bufferLen);
        
        struct sockaddr_un srcAddr;
        socklen_t srcAddrLen;
        
        while(1){
            fprintf(stderr, "Thread/0x%08x/in : Waiting for message from network\n", threadId);     
            memset(&srcAddr, 0, sizeof(srcAddr));
            srcAddrLen=sizeof(srcAddr);
            auto len=recvfrom(mboxSocket, buffer, bufferLen, 0, (struct sockaddr *)&srcAddr, &srcAddrLen);
            if(len==-1){
                perror("recvfrom/error");
                exit(1);
            }
            if(len>(int)bufferLen){
                perror("recvfrom/size");
                exit(1);
            }
            
            fprintf(stderr, "Thread/0x%08x/in : Received message of length %u from socket address '%s'\n", threadId, len, srcAddr.sun_path);     
            
            uint32_t srcThreadId;
            if(0){
                // Get the thread id out of the address for sanity purposes...                
                if(1 != sscanf(srcAddr.sun_path, addrTemplate, &srcThreadId)){
                    fprintf(stderr, "Thread/0x%08x/in : Couldn't parse threadId from socket address '%s'\n", threadId, srcAddr.sun_path);
                    puts(srcAddr.sun_path);
                    exit(1);
                }
            }else{
                // cygwin doesn't fill in the source path...
                srcThreadId=((packet_t*)buffer)->source.thread;
            }
            uint32_t dstThreadId=((packet_t*)buffer)->dest.thread;

            fprintf(stderr, "Thread/0x%08x/in : delivering message of length %u from thread 0x%08x to thread 0x%08x\n", threadId, len, srcThreadId, dstThreadId);                 
            while(!mbox.netTryPushMessage(dstThreadId, len, buffer)){
                sched_yield();
            }
        }           
    });
    
    std::thread outgoing([&](){
        fprintf(stderr, "Thread/0x%08x/out : Starting outgoing thread\n", threadId);     
        
        unsigned bufferLen=mbox_t::WordsPerMsg*4;
        void *buffer=malloc(bufferLen);
        
        struct sockaddr_un srcAddr;
        socklen_t srcAddrLen;
        
        while(1){
            uint32_t dstThreadId;
            uint32_t byteLength;
            
            fprintf(stderr, "Thread/0x%08x/out : Waiting to pull message from mailbox\n", threadId);     
            while(!mbox.netTryPullMessage(dstThreadId, byteLength, buffer)){
                sched_yield();
            }
            
            assert(byteLength <= 4*mbox_t::WordsPerMsg);
            
            fprintf(stderr, "Thread/0x%08x/out : Pulled message to %08x of length %u\n", threadId, dstThreadId, byteLength);     
            
            srcAddrLen=sizeof(srcAddr);
            srcAddr.sun_family=AF_UNIX;
            if(107 <= snprintf(srcAddr.sun_path, 107, addrTemplate, dstThreadId)){
                fprintf(stderr, "Thread/0x%08x/out : Couldn't create address for threadId %08x\n", threadId, dstThreadId);
                fprintf(stderr, addrTemplate, dstThreadId);
                exit(1);
            }
            
            auto len=sendto(mboxSocket, buffer, byteLength, 0, (struct sockaddr *)&srcAddr, srcAddrLen);
            if(len==-1){
                perror("sendto/error");
                exit(1);
            }
            if(len!=(int)byteLength){
                perror("sendto/size");
                exit(1);
            }
            fprintf(stderr, "Thread/0x%08x/out : Send message to %08x\n", threadId, dstThreadId);     

        }
    });
   
    sleep(1);

    fprintf(stderr, "Thread/0x%08x/cpu : Starting soft-switch\n", threadId);     
    softswitch_main();
    
    return 0;
}
