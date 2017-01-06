
#error "WIP"

template<
    unsigned LogMsgsPerThread,
    unsigned LogWordsPerFlit,
    unsigned LogMaxFlitsPerMsg
>
class TinselMbox
{
public:
    const unsigned MsgsPerThread = 1<<LogMsgsPerThread;
    const unsigned FlitsPerMsg = 1<<LogMaxFlitsPerMsg;
    const unsigned WordsPerMsg = 1<<(LogWordsPerFlit + LogMaxFlitsPerMsg);
private:
    enum MsgStatus
    {
        SoftwareOwned,
        HardwareSend,
        HardwareEmpty,
        HardwareFull
    };

    uint32_t m_myThreadId;

    struct Slot
    {
        MsgStatus status;
        uint32_t buffer[WordsPerMessage];
    };
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
            if(m_slots[i].data==p){
                return i;
            }
        }
        assert(0);
    }
public:

    //////////////////////////////////////////
    // Tinsel side

    void *mboxAlloc(unsigned n) const
    {
        return m_slots[n].buffer;
    }

    void mboxCanRecv() const
    {
        lock_t lock(m_mutex);
        return m_numFull>0;
    }

    void *mboxRecv(unsigned n)
    {
        lock_t lock(m_mutex);

        assert(mboxCanRecv());
        for(unsigned i=0; i<MsgsPerThread; i++){
            if(m_slots[i].status==HardwareFull){
                m_slots[i].status=SoftwareOwned;
                m_numFull--;
                return m_slots[i].data;
            }
        }
        assert(0);
    }

    void mboxAlloc(void *p)
    {
        lock_t lock(m_mutex);

        unsigned index=findSlot(p);
        assert(m_slots[index].status==SoftwareOwned);
        m_slots[index].status=HardwareEmpty;
        m_numEmpty++;
    }

    bool mboxCanSend() const
    {
        lock_t lock(m_mutex);
        assert(m_sendActive ? m_slots[m_sendSlot].status==HardwareSend : 1 );
        return !m_sendActive;
    }


    void mboxSend(uint32_t address, void *p)
    {
        lock_t lock(m_mutex);

        assert(mboxCanSend());

        unsigned index=findSlot(p);
        m_sendActive=true;
        m_sendAddress=address;
        m_sendSlot=index;
        m_sendLength=
    }

    //////////////////////////////////////////////////
    // Network side

    bool netTrySend(uint32_t &threadId, void *data)
    {
        lock_t lock(m_mutex);
    }

    void netTryRecv(uint32_t threadId, const void *data)
    {
        lock_t lock(m_mutex);

        assert(threadId==m_myThreadId);

        if(m_numSlotsEmpty == 0)
            return false;

        for(unsigned i=0; i<
    }
};
