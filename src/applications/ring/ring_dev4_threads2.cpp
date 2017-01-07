#include "ring.hpp"

const unsigned THREAD_COUNT=2;

const unsigned DEVICE_INSTANCE_COUNT_thread0=2;
const unsigned DEVICE_INSTANCE_COUNT_thread1=2;

// A graph of four devices, two in thread 0, and two in thread 1.

dev_state dev0_state{
    1
};
address_t dev0_out_addresses[1]={
    {
        0, // thread
        1, // device
        INPUT_INDEX_dev_in
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
        1, // thread
        0, // device
        INPUT_INDEX_dev_in
    }
};
OutputPortTargets dev1_targets[1]={
    {
        1,
        dev1_out_addresses
    }
};
dev_state dev2_state{
    0
};
address_t dev2_out_addresses[1]={
    {
        1, // thread
        1, // device
        INPUT_INDEX_dev_in
    }
};
OutputPortTargets dev2_targets[1]={
    {
        1,
        dev2_out_addresses
    }
};

dev_state dev3_state{
    0
};
address_t dev3_out_addresses[1]={
    {
        0, // thread
        0, // device
        INPUT_INDEX_dev_in
    }
};
OutputPortTargets dev3_targets[1]={
    {
        1,
        dev3_out_addresses
    }
};


DeviceContext DEVICE_INSTANCE_CONTEXTS_thread0[DEVICE_INSTANCE_COUNT_thread0]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev0_state,
        0, // device index
        dev0_targets, // address lists for ports
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev1_state,
        0, // device index
        dev1_targets, // address lists for ports
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
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_dev, // vtable
        0, // no properties
        &dev3_state,
        0, // device index
        dev3_targets, // address lists for ports
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
        &inst0_props,
        DEVICE_TYPE_COUNT,
        DEVICE_TYPE_VTABLES,
        DEVICE_INSTANCE_COUNT_thread0,
        DEVICE_INSTANCE_CONTEXTS_thread0,
        0, // rtsHead
        0, // rtsTail
        0, // rtcChecked
        0  // rtcOffset
    },
    {
        &inst0_props,
        DEVICE_TYPE_COUNT,
        DEVICE_TYPE_VTABLES,
        DEVICE_INSTANCE_COUNT_thread1,
        DEVICE_INSTANCE_CONTEXTS_thread1,
        0, // rtsHead
        0, // rtsTail
        0, // rtcChecked
        0  // rtcOffset
    }

};

