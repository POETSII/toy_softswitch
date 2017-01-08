#include "edge_props.hpp"

const unsigned THREAD_COUNT=1;

const unsigned DEVICE_INSTANCE_COUNT=4;

const unsigned DEVICE_INSTANCE_COUNT_thread0=4;

graph_props inst0_props{
};

outer_state dev0_state{
    1
};
address_t dev0_out_addresses[]={
    {
        0, // thread
        3, // device
        INPUT_INDEX_inner_in
    }
};
InputPortSources dev0_sources[]={
};
OutputPortTargets dev0_targets[]={
    {
        1,
        dev0_out_addresses
    }
};

outer_state dev1_state{
    1
};
address_t dev1_out_addresses[]={
    {
        0, // thread
        3, // device
        INPUT_INDEX_inner_in
    }
};
InputPortSources dev1_sources[]={
};
OutputPortTargets dev1_targets[]={
    {
        1,
        dev1_out_addresses
    }
};

outer_state dev2_state{
    1
};
address_t dev2_out_addresses[]={
    {
        0, // thread
        3, // device
        INPUT_INDEX_inner_in
    }
};
InputPortSources dev2_sources[]={
};
OutputPortTargets dev2_targets[]={
    {
        1,
        dev2_out_addresses
    }
};

inner_props dev3_props{
    DEVICE_INSTANCE_COUNT-1
};
inner_state dev3_state{
    0
};
address_t dev3_out_addresses[]={
};
inner_in_props dev3_in_dev0={
    0x1
};
inner_in_props dev3_in_dev1={
    0x2
};
inner_in_props dev3_in_dev2={
    0x4
};
InputPortBinding dev3_in_bindings[]={
    {
        {
            0, // thread
            0, // device
            0 // port
        },
        &dev3_in_dev0,
        0
    },{
        {
            0, // thread
            1, // device
            0 // port
        },
        &dev3_in_dev1,
        0
    },{
        {
            0, // thread
            2, // device
            0 // port
        },
        &dev3_in_dev2,
        0
    }      
};
InputPortSources dev3_sources[]={
    {
        3,
        dev3_in_bindings
    }
};
OutputPortTargets dev3_targets[]={
};





DeviceContext DEVICE_INSTANCE_CONTEXTS_thread0[DEVICE_INSTANCE_COUNT_thread0]={
    {
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_outer, // vtable
        0, // no properties
        &dev0_state,
        0, // device index
        dev0_targets, // address lists for ports
        dev0_sources, // source list for inputs
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_outer, // vtable
        0, // no properties
        &dev1_state,
        1, // device index
        dev1_targets, // address lists for ports
        dev1_sources, // source list for inputs
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_outer, // vtable
        0, // no properties
        &dev2_state,
        2, // device index
        dev2_targets, // address lists for ports
        dev2_sources, // source list for inputs
        0, // rtsFlags
        false, // rtc
        0,  // prev
        0   // next
    },{
        DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_inner, // vtable
        &dev3_props,
        &dev3_state,
        3, // device index
        dev3_targets, // address lists for ports
        dev3_sources, // source list for inputs
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
        0  // rtcOffset
    }
};

    
