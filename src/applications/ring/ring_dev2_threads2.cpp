#include "ring.hpp"

const unsigned THREAD_COUNT=2;

const unsigned DEVICE_INSTANCE_COUNT_thread0=1;
const unsigned DEVICE_INSTANCE_COUNT_thread1=1;

// A graph of two devices. Two threads, one device in each

dev_state dev0_state{
    1
};
address_t dev0_out_addresses[1]={
    {
        1, // thread 1
        0, // device index 0
        INPUT_INDEX_dev_in
    }
};
InputPortSources dev0_sources[1]={
    {
        0,
        0
    }
};
OutputPortTargets dev0_targets[1]={
    {
        1,
        dev0_out_addresses
    }
};

dev_state dev1_state{
    0
};
address_t dev1_out_addresses[1]={
    {
        0, // thread 0
        0, // device index 0
        INPUT_INDEX_dev_in
    }
};
InputPortSources dev1_sources[1]={
    {
        0,
        0
    }
};
OutputPortTargets dev1_targets[1]={
    {
        1,
        dev1_out_addresses
    }
};

DeviceContext DEVICE_INSTANCE_CONTEXTS_thread0[1]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev0_state,
        0, // device index
        dev0_targets, // address lists for ports
        dev0_sources,
        "dev0",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    }
};

DeviceContext DEVICE_INSTANCE_CONTEXTS_thread1[1]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev1_state,
        0, // device index
        dev1_targets, // address lists for ports
        dev1_sources,
        "dev1",
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    }
};

graph_props inst0_props{
    2 // increment by 2
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
        0, // rtcOffset
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
    }

};

