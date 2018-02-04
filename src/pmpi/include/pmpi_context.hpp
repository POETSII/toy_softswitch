#ifndef pmpi_context_hpp
#define pmpi_context_hpp

#include "tinsel_api.hpp"

#include "softswitch.hpp"

#include <cstring>

typedef uint32_t MPI_Comm;

const MPI_Comm MPI_COMM_WORLD = -1;

enum MPI_Datatype{
    MPI_BYTE                   = 0x4c00010d
};

const int MPI_ANY_SOURCE = -1;
const int MPI_ANY_TAG = -1;

// TODO : this should come from the tinsel config
const int PMPI_MAX_SLOTS = 7;

typedef struct _MPI_Status {
  int count;
  //int cancelled;
  int MPI_SOURCE;
  int MPI_TAG;
  //int MPI_ERROR;
} MPI_Status, *PMPI_Status;

struct pmpi_message_t
{
    // TODO : This is needed for compatibility with current tinsel software implementations, but
    // is not actually required in hardware
    uint32_t destAddress; 
    
    int dest : 16;
    int source : 16;
    int tag     : 16;
    int size    : 16;
    
    uint8_t payload[];
};

class pmpi_context_t
{
private:

    int m_size = -1;
    int m_rank = -1;

    // Global table which maps an mpi rank to the thread where it
    // lives. We actually never need the inverse, as rank is always
    // embedded in messages.
    const uint32_t *m_rankToAddress = 0;

    // Pointers to mbox slots that contain an incoming message
    pmpi_message_t *m_buffers[PMPI_MAX_SLOTS] = {0};
    // Buffers in the range [0,validPoint) contain messages to be received
    int m_validPoint = {-1}; 

    pmpi_message_t *m_send_slot = 0;

    bool matches(int source, int tag, const pmpi_message_t *msg)
    {
        return (source==MPI_ANY_SOURCE || source==msg->source) && (tag==MPI_ANY_TAG || tag==msg->tag);
    }

    pmpi_message_t *process_events_till_can_recv(int source, int tag)
    {
        while(1){
            tinsel_mboxWaitUntil(tinsel_CAN_RECV);
            
            pmpi_message_t *msg=(pmpi_message_t *)tinsel_mboxRecv();
            if(matches(source, tag, msg)){
                return msg;
            }
            assert(m_validPoint < PMPI_MAX_SLOTS+1);
            m_buffers[m_validPoint++]=msg;
        }
    }

    void process_events_till_can_send()
    {
        while(1){
            tinsel_mboxWaitUntil( (tinsel_WakeupCond)(tinsel_CAN_RECV|tinsel_CAN_SEND));
            
            if(tinsel_mboxCanSend()){
                return ;
            }else{
                pmpi_message_t *msg=(pmpi_message_t *)tinsel_mboxRecv();
                assert(m_validPoint < PMPI_MAX_SLOTS+1);
                m_buffers[m_validPoint++]=msg;
            }
        }
    }

  
  public:
    // rankToAddress needs to have infinite lifetime
    int MPI_Init(int size, int rank, const uint32_t *rankToAddress)
    {
        assert(m_rank==-1);
        
        m_size=size;
        m_rank=rank;
        m_rankToAddress=rankToAddress;
        m_validPoint=0;
   
        m_send_slot=(pmpi_message_t*)tinsel_mboxSlot(0);
        for(unsigned i=1; i<tinsel_mboxSlotCount(); i++){
            tinsel_mboxAlloc( tinsel_mboxSlot(i) );
        }
        
        return 0;
    }
    
    int MPI_Finalize()
    {
        assert(m_rank!=-1);
        m_rank=-1;
    }
    
    int MPI_Comm_rank(MPI_Comm comm, int *rank)
    {
        assert(comm==MPI_COMM_WORLD);
        *rank=m_rank;
        return 0;
    }

    int MPI_Comm_size( MPI_Comm comm, int *size ) 
    {
        assert(comm==MPI_COMM_WORLD);
        *size=m_size;
        return 0;
    }

    int MPI_Send(
        const void *buf,
        int count,
        MPI_Datatype datatype, // Always byte
        int dest,
        int tag,
        MPI_Comm comm
    ){
        assert(datatype==MPI_BYTE);
        assert(comm==MPI_COMM_WORLD);
        assert(dest < m_size);

        if(!tinsel_mboxCanSend()){
            process_events_till_can_send();
        }

        memcpy(m_send_slot->payload, buf, count);
        m_send_slot->dest=dest;
        m_send_slot->source=m_rank;
        m_send_slot->tag=tag;
        m_send_slot->size=sizeof(pmpi_message_t)+count;
        
        // TODO : This shouldn't be needed in hardware. It is todo with the 
        // software emulation capabilities.
        m_send_slot->destAddress=m_rankToAddress[dest];

        tinsel_mboxSetLen(m_send_slot->size);
        tinsel_mboxSend(m_rankToAddress[dest], m_send_slot);
        
        return 0;
    }

    int MPI_Recv(
        void *buf,
        int count,
        MPI_Datatype datatype,
        int source,
        int tag,
        MPI_Comm comm,
        MPI_Status *status
    ){
        assert(datatype==MPI_BYTE);

        pmpi_message_t *msg=0;

        if(m_validPoint){
            if(source==MPI_ANY_SOURCE && tag==MPI_ANY_TAG){
                msg=m_buffers[--m_validPoint];
            }else{
                for(int i=0; i<m_validPoint; i++){
                    auto slot=m_buffers[i];
                    if(matches(source,tag,slot)){
                        msg=slot;
                        m_buffers[i]=m_buffers[--m_validPoint];
                        break;
                    }
                }
            }
        }
        
        if(!msg){
            msg=process_events_till_can_recv(source, tag);
        }

        unsigned payloadSize=msg->size-sizeof(pmpi_message_t);
        assert( payloadSize <= count );

        memcpy(buf, msg->payload, payloadSize);
        status->count=payloadSize;
        status->MPI_SOURCE=msg->source;
        status->MPI_TAG=msg->tag;
        
        tinsel_mboxAlloc(msg);

        return 0;
    }
};

#endif
