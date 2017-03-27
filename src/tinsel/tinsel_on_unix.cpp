#include "tinsel_mbox.hpp"

const unsigned LogMsgsPerThread = 4;
const unsigned LogWordsPerFlit = 2;
const unsigned LogMaxFlitsPerMsg = 4;
    
typedef TinselMbox<LogMsgsPerThread,LogWordsPerFlit,LogMaxFlitsPerMsg> mbox_t;

static thread_local mbox_t *mbox = 0;

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <cstdarg>
#include <getopt.h>

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


void mbox_thread(const char *socketDir, uint32_t threadId, int logLevel)
{
    char addrTemplate[128];
    snprintf(addrTemplate, sizeof(addrTemplate), "%s/%%08xl", socketDir);
    puts(addrTemplate);
    
    mbox=new mbox_t();
    
    // Take local copy so that our worker threads don't
    // end up on a different variable.
    mbox_t *mboxLocal=mbox;
    
    mbox->init(threadId,logLevel);
        
    struct sockaddr_un mboxAddr;
    memset(&mboxAddr,0,sizeof(sockaddr_un));
    mboxAddr.sun_family=AF_UNIX;
    snprintf(mboxAddr.sun_path, 107, addrTemplate, threadId);

    mbox->log(1, "Thread/0x%08x : Opening socket %s\n", threadId, mboxAddr.sun_path); 
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
        try{        
            mboxLocal->log(1,"/ in : Starting incoming thread", threadId);     
            
            unsigned bufferLen=mbox_t::WordsPerMsg*4;
            void *buffer=malloc(bufferLen);
            
            struct sockaddr_un srcAddr;
            socklen_t srcAddrLen;
            
            while(1){
                mboxLocal->log(2,"/ in : Waiting for message from network", threadId);     
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
                
                uint32_t srcThreadId;
                srcThreadId=((packet_t*)buffer)->source.thread;
                uint32_t dstThreadId=((packet_t*)buffer)->dest.thread;

                mboxLocal->log(2, "/ in : delivering message of length %u from thread 0x%08x to thread 0x%08x", threadId, len, srcThreadId, dstThreadId);                 
                mboxLocal->netPushMessage(dstThreadId, len, buffer);
            }           
        }catch(std::exception &e){
            fprintf(stderr, "Thread/0x%08x/in : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });
    
    
    std::thread outgoing([&](){
        try{
            mboxLocal->log(1, "/ out : Starting outgoing thread", threadId);     
            
            unsigned bufferLen=mbox_t::WordsPerMsg*4;
            void *buffer=malloc(bufferLen);
            
            struct sockaddr_un srcAddr;
            socklen_t srcAddrLen;
            
            while(1){
                uint32_t dstThreadId=-1;
                uint32_t byteLength=-1;
                
                mboxLocal->log(2, "/out : Waiting to pull message from mailbox", threadId);     
                mboxLocal->netPullMessage(dstThreadId, byteLength, buffer);
                
                assert(byteLength <= 4*mbox_t::WordsPerMsg);
                
                mboxLocal->log(2, "/out : Pulled message to %08x of length %u", threadId, dstThreadId, byteLength);     
                
                srcAddrLen=sizeof(srcAddr);
                srcAddr.sun_family=AF_UNIX;
                if(107 <= snprintf(srcAddr.sun_path, 107, addrTemplate, dstThreadId)){
                    fprintf(stderr, "Thread/0x%08x/out : Couldn't create address for threadId %08x", threadId, dstThreadId);
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
                mboxLocal->log(2, "/ out : Send message to %08x", threadId, dstThreadId);     

            }
        }catch(std::exception &e){
            fprintf(stderr, "Thread/0x%08x/out : Exception : %s\n", threadId, e.what());
            exit(1);
        }
    });

    mboxLocal->log(1,"Thread/0x%08x/cpu : Starting soft-switch", threadId);     
    softswitch_main();

    incoming.join();
    outgoing.join();

}




int main(int argc, char *argv[])
{
    try{
        
        std::string socketDir("/tmp/tinsel_on_unix.XXXXXXXX");
        
        uint32_t threadId=-1;
        
        bool useThreads=true;
        
        int applLogLevel=5;
        int softLogLevel=5;
        int hardLogLevel=5;
        
        
        while (1){
          static struct option long_options[] =
            {
              {"appl-log-level",    required_argument,    0, 'a'},
              {"soft-log-level",    required_argument,    0, 's'},
              {"hard-log-level",    required_argument,    0, 'h'},
              {"socket-dir",        optional_argument,    0, 'd'},
              {0, 0, 0, 0}
            };
            int option_index = 0;

            int c = getopt_long (argc, argv, "a:s:h:d:", long_options, &option_index);
            if (c == -1){
                break;
            }

            switch (c)
            {
            case 'a':  applLogLevel=atoi(optarg); break;
            case 's':  softLogLevel=atoi(optarg); break;
            case 'h':  hardLogLevel=atoi(optarg); break;
            case 'd':  socketDir=optarg; break;
            case '?':   break;
            default:    exit(1);
            }
        }
        
        for(int i=0; i<softswitch_pthread_count; i++){
            softswitch_pthread_contexts[i].applLogLevel=applLogLevel;
            softswitch_pthread_contexts[i].softLogLevel=softLogLevel;
            softswitch_pthread_contexts[i].hardLogLevel=hardLogLevel;
        }
        
        //////////////////////////////////////////
        // Check the template, and make socket directory
        
        int numXs=0;
        auto rit=socketDir.rbegin();
        while(rit!=socketDir.rend()){
            if(*rit!='X')
                break;
            numXs++;
            ++rit;
        }
        if(numXs==0){
            socketDir+=".";
        }
        while(numXs<8){
            socketDir+="X";
            numXs++;
        }
        
        socketDir.c_str();
        mkdtemp (&socketDir[0]);
        
        
        ////////////////////////////////////////////////
        
        if(softswitch_pthread_count==1){
            mbox_thread(socketDir.c_str(), 0, hardLogLevel);
        }else if(!useThreads){
            for(threadId=0; threadId<softswitch_pthread_count-1; threadId++){
                auto pid=fork();
                if(pid!=0){
                    break;
                }
            }
            
            mbox_thread(socketDir.c_str(), threadId, hardLogLevel);
        }else{
            std::vector<std::thread> threads;
            for(unsigned i=0; i<softswitch_pthread_count; i++){
                uint32_t threadId=i;
                threads.emplace_back( [&,threadId](){ mbox_thread(socketDir.c_str(), threadId, hardLogLevel); } );
            }
            
            for(unsigned i=0; i<softswitch_pthread_count; i++){
                threads[i].join();
            }
        }
    }catch(std::exception &e){
        fprintf(stderr, "Exception: %s\n", e.what());
        throw;
    }
    
    return 0;
}
