#include "barrier.hpp"

const unsigned THREAD_COUNT=3;

const unsigned DEVICE_INSTANCE_COUNT=5;

const unsigned DEVICE_INSTANCE_COUNT_thread0=2;
const unsigned DEVICE_INSTANCE_COUNT_thread1=2;
const unsigned DEVICE_INSTANCE_COUNT_thread2=1;


graph_props inst0_props{
    100, // maxTime
    DEVICE_INSTANCE_COUNT-1  // devCount
};

dev_state dev0_state{
    0,
    DEVICE_INSTANCE_COUNT-1,
    0,
    false
};
address_t dev0_out_addresses[]={
    {
        0, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        0, // thread
        1, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        1, // device
        INPUT_INDEX_dev_in
    }
};
address_t dev0_halt_addresses[]={
    {
        2, // thread
        0, // device
        INPUT_INDEX_halt_in
    }
};
InputPortSources dev0_sources[]={
    {
        0,
        0
    }
};
OutputPortTargets dev0_targets[]={
    {
        4,
        dev0_out_addresses
    },{
        1,
        dev0_halt_addresses
    }
};

dev_state dev1_state{
    0,
    DEVICE_INSTANCE_COUNT-1,
    0,
    false
};
address_t dev1_out_addresses[]={
    {
        0, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        0, // thread
        1, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        1, // device
        INPUT_INDEX_dev_in
    }
};
address_t dev1_halt_addresses[]={
    {
        2, // thread 
        0, // device
        INPUT_INDEX_halt_in
    }
};
InputPortSources dev1_sources[]={
    {
        0,
        0
    }
};
OutputPortTargets dev1_targets[]={
    {
        4,
        dev1_out_addresses
    },{
        1,
        dev1_halt_addresses
    }
};

dev_state dev2_state{
    0,
    DEVICE_INSTANCE_COUNT-1,
    0,
    false
};
address_t dev2_out_addresses[]={
    {
        0, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        0, // thread
        1, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        1, // device
        INPUT_INDEX_dev_in
    }
};
address_t dev2_halt_addresses[]={
    {
        2, // thread 
        0, // device
        INPUT_INDEX_halt_in
    }
};
InputPortSources dev2_sources[]={
    {
        0,
        0
    }
};
OutputPortTargets dev2_targets[]={
    {
        4,
        dev2_out_addresses
    },{
        1,
        dev2_halt_addresses
    }
};

dev_state dev3_state{
    0,
    DEVICE_INSTANCE_COUNT-1,
    0,
    false
};
address_t dev3_out_addresses[]={
    {
        0, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        0, // thread
        1, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        0, // device
        INPUT_INDEX_dev_in
    },{
        1, // thread
        1, // device
        INPUT_INDEX_dev_in
    }
};
address_t dev3_halt_addresses[]={
    {
        2, // thread 
        0, // device
        INPUT_INDEX_halt_in
    }
};
InputPortSources dev3_sources[]={
    {
        0,
        0
    }
};
OutputPortTargets dev3_targets[]={
    {
        4,
        dev3_out_addresses
    },{
        1,
        dev3_halt_addresses
    }
};


halt_state dev4_state{
    0
};
InputPortSources dev4_sources[]={
    0,
    0
};
OutputPortTargets dev4_targets[]={
};

DeviceContext DEVICE_INSTANCE_CONTEXTS_thread0[DEVICE_INSTANCE_COUNT_thread0]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev0_state,
        0, // device index
        dev0_targets, // address lists for ports
        dev0_sources, // source list for edge info
        "dev0",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0,   // next
        
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev1_state,
        1, // device index
        dev1_targets, // address lists for ports
        dev1_sources, // source list for edge info
        "dev1",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    }
};

DeviceContext DEVICE_INSTANCE_CONTEXTS_thread1[DEVICE_INSTANCE_COUNT_thread1]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev2_state,
        0, // device index
        dev2_targets, // address lists for ports
        dev2_sources,
        "dev2",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev3_state,
        1, // device index
        dev3_targets, // address lists for ports
        dev3_sources, // address lists for ports
        "dev3",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    }
};

DeviceContext DEVICE_INSTANCE_CONTEXTS_thread2[DEVICE_INSTANCE_COUNT_thread2]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_halt, // vtable
        0, // no properties
        &dev4_state,
        0, // device index
        dev4_targets, // address lists for ports
        dev4_sources,
        "dev4",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    }
};



unsigned softswitch_pthread_count = THREAD_COUNT;

PThreadContext softswitch_pthread_contexts[THREAD_COUNT]=
{
    {
        0,
        &inst0_props,
        DEVICE_TYPE_COUNT,
        DEVICE_TYPE_VTABLES,
        DEVICE_INSTANCE_COUNT_thread0,
        DEVICE_INSTANCE_CONTEXTS_thread0,
        0, // lamport
        0, // rtsHead
        0, // rtsTail
        0, // rtcChecked
        0,  // rtcOffset
        0,
        0,
        0,
        0,
        0,
        0
    },
    {
        1,
        &inst0_props,
        DEVICE_TYPE_COUNT,
        DEVICE_TYPE_VTABLES,
        DEVICE_INSTANCE_COUNT_thread1,
        DEVICE_INSTANCE_CONTEXTS_thread1,
        0, // lamport
        0, // rtsHead
        0, // rtsTail
        0, // rtcChecked
        0,  // rtcOffset
        0,
        0,
        0,
        0,
        0,
        0
    },
    {
        2,
        &inst0_props,
        DEVICE_TYPE_COUNT,
        DEVICE_TYPE_VTABLES,
        DEVICE_INSTANCE_COUNT_thread2,
        DEVICE_INSTANCE_CONTEXTS_thread2,
        0, // lamport
        0, // rtsHead
        0, // rtsTail
        0, // rtcChecked
        0,  // rtcOffset
        0,
        0,
        0,
        0,
        0,
        0
    }

};

    
