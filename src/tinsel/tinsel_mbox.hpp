#ifndef tinsel_mbox_hpp
#define tinsel_mbox_hpp

#include <cstdint>
#include <cassert>
#include <mutex>
#include <thread>
#include <cstring>
#include <condition_variable>

#include "tinsel_api.hpp"

template<
    unsigned LogMsgsPerThread,
    unsigned LogWordsPerFlit,
    unsigned LogMaxFlitsPerMsg
>
class TinselMbox
{
public:
    static const unsigned MsgsPerThread = 1<<LogMsgsPerThread;
    static const unsigned FlitsPerMsg = 1<<LogMaxFlitsPerMsg;
    static const unsigned WordsPerMsg = 1<<(LogWordsPerFlit + LogMaxFlitsPerMsg);
private:
    typedef std::mutex mutex_t;
    typedef std::unique_lock<mutex_t> lock_t;
    typedef std::condition_variable condition_variable_t;

    enum MsgStatus
    {
        SoftwareOwned,
        HardwareSend,
        HardwareEmpty,
        HardwareFull
    };
    
    struct Slot
    {
        MsgStatus status;
        uint32_t buffer[WordsPerMsg];
    };
    
    mutable mutex_t m_mutex;
    condition_variable_t m_condVar;

    uint32_t m_myThreadId;
    
    Slot m_slots[MsgsPerThread];

    unsigned m_numSlotsEmpty;
    unsigned m_numSlotsFull;

    bool m_sendActive;
    uint32_t m_sendAddress;
    uint32_t m_sendSlot;
    uint32_t m_sendLength;


    unsigned findSlot(const void *p)
    {
        for(unsigned i=0; i<MsgsPerThread; i++){
            if(m_slots[i].buffer==p){
                return i;
            }
        }
        assert(0);
    }
    
public:
    
    void init(uint32_t threadId)
    {
        m_myThreadId=threadId;
        m_numSlotsEmpty=0;
        m_numSlotsFull=0;
        m_sendActive=false;
        
        // All slots start with software
        for(unsigned i=0; i<MsgsPerThread; i++){
            m_slots[i].status=SoftwareOwned;
        }
    }

    //////////////////////////////////////////
    // Tinsel side
    
    uint32_t getThreadId() const
    { return m_myThreadId; }

    //! Just gets the pointer, so no lock needed
    void *mboxSlot(unsigned n) const
    {
        // Claiming this is const member returning non-const pointer,
        // as it doesn't change state of the mailbox.
        return (void*)m_slots[n].buffer;
    }

    bool mboxCanRecv() const
    {
        lock_t lock(m_mutex);
        return m_numSlotsFull>0;
    }

    void *mboxRecv()
    {
        lock_t lock(m_mutex);

        assert(m_numSlotsFull>0);
        
        unsigned i;
        for(i=0; i<MsgsPerThread; i++){
            if(m_slots[i].status==HardwareFull){
                break;
            }
        }
        assert(m_slots[i].status==HardwareFull);
        
        m_slots[i].status=SoftwareOwned;
        m_numSlotsFull--;
        
        // nobody waiting on the lock needs to be notified
        
        return m_slots[i].buffer;
    }

    void mboxAlloc(void *p)
    {
        lock_t lock(m_mutex);

        unsigned index=findSlot(p);
        assert(m_slots[index].status==SoftwareOwned);
        m_slots[index].status=HardwareEmpty;
        m_numSlotsEmpty++;
        
        if(m_numSlotsEmpty==1){
            // Only if we go from 0 to 1 does anyone need to know 
            m_condVar.notify_all();
        }
    }

    bool mboxCanSend() const
    {
        lock_t lock(m_mutex);
        assert(m_sendActive ? m_slots[m_sendSlot].status==HardwareSend : 1 );
        return !m_sendActive;
    }

    
    void mboxSetLen(uint32_t byteLength)
    {
        lock_t lock(m_mutex);
        
        // need to be under lock to make sure it isn't breaking condition on
        // changing registers while send is under way
        
        assert(!m_sendActive);
        
        m_sendLength=byteLength;
    }

    void mboxSend(uint32_t address, void *p)
    {
        lock_t lock(m_mutex);

        assert(!m_sendActive);

        unsigned index=findSlot(p);
        m_sendActive=true;
        m_sendAddress=address;
        m_sendSlot=index;
        m_slots[index].status=HardwareSend;
        
        // anyone trying to pull needs to know, and we must be going from !sendActive -> sendActive
        m_condVar.notify_all();
    }
    
    void mboxWaitUntil(tinsel_WakeupCond cond)
    {
        assert(cond); // If condition is non-empty we'll lock up
        
        lock_t lock(m_mutex);
        
        m_condVar.wait(lock, [&](){
            if( (cond&TINSEL_CAN_SEND) && (m_sendActive==false) ){
                return true;
            }
            if( (cond&TINSEL_CAN_RECV) && (m_numSlotsFull>0) ){
                return true;
            }
            return false;
        });
    }

    //////////////////////////////////////////////////
    // Network side
    
    bool netPullMessage(uint32_t &threadId, unsigned &byteLength, void *data, bool block=true)
    {
        lock_t lock(m_mutex);
        
        if(m_sendActive==false){
            if(block){
                m_condVar.wait(lock, [&](){ return m_sendActive; });
            }else{
                return false;
            }
        }
        
        assert(m_sendLength <= 4*WordsPerMsg);
        assert(m_slots[m_sendSlot].status==HardwareSend);

        memcpy(data, m_slots[m_sendSlot].buffer, m_sendLength);
        m_slots[m_sendSlot].status=SoftwareOwned;
        threadId=m_sendAddress;
        byteLength=m_sendLength;
        m_sendActive=false;

        // Anyone in waitUntil(CAN_SEND) needs to know
        m_condVar.notify_all();
        
        return true;
    }

    bool netTryPullMessage(uint32_t &threadId, unsigned &byteLength, void *data)
    {
        return netPullMessage(threadId, byteLength, data, false);
    }

    bool netPushMessage(uint32_t dstThreadId, uint32_t byteLength, const void *data, bool block=true)
    {
        lock_t lock(m_mutex);

        assert(dstThreadId==m_myThreadId); // sanity check

        assert(byteLength <= 4*WordsPerMsg);
        
        if(m_numSlotsEmpty == 0){
            if(block){
                m_condVar.wait(lock, [&](){ return m_numSlotsEmpty>0; });
            }else{
                return false;
            }
        }

        // Find a slot
        unsigned start=rand()%MsgsPerThread;
        unsigned sel;
        for(unsigned i=0; i<MsgsPerThread; i++){
            sel=(i+start)%MsgsPerThread;
            if(m_slots[sel].status==HardwareEmpty)
                break;
        }

        assert (m_slots[sel].status==HardwareEmpty);

        // Copy message in
        memcpy(m_slots[sel].buffer, data, byteLength);
        m_slots[sel].status=HardwareFull;
        m_numSlotsFull++;
        
        // Anyone in a waitUntil(CAN_RECV) needs to know
        m_condVar.notify_all();

        return true;
    }
    
    bool netTryPushMessage(uint32_t dstThreadId, uint32_t byteLength, const void *data)
    {
        return netPushMessage(dstThreadId, byteLength, data, false);
    }
};

#endif
