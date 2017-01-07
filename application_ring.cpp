#include "softswitch.hpp"

#include <cstdio>

///////////////////////////////////////////////
// Graph type stuff

const unsigned DEVICE_TYPE_COUNT = 1;
const unsigned DEVICE_TYPE_INDEX_dev = 0;

const unsigned INPUT_COUNT_dev = 1;
const unsigned INPUT_INDEX_dev_in = 0;

const unsigned OUTPUT_COUNT_dev = 1;
const unsigned OUTPUT_INDEX_dev_out = 0;
const unsigned OUTPUT_FLAG_dev_out = 1<<OUTPUT_INDEX_dev_out;

struct graph_props
{
    uint32_t increment;
};

struct token_msg
{
    uint32_t count;
};

// No properties
struct dev_props
{
};

struct dev_state
{
    uint32_t count;
};

uint32_t dev_ready_to_send_handler(
    const graph_props *gProps,
    const dev_props *dProps,
    const dev_state *dState
){
    return dState->count ? OUTPUT_FLAG_dev_out : 0;
}

bool dev_in_receive_handler(
    const graph_props *gProps,
    const dev_props *dProps,
    dev_state *dState,
    const void *eProps,
    void *eState,
    token_msg *msg
){
    fprintf(stderr, "ring_dev_in:     begin\n");    
    fprintf(stderr, "ring_dev_in:       gProps=%p\n", gProps);    
    fprintf(stderr, "ring_dev_in:       dProps=%p\n", dProps);    
    fprintf(stderr, "ring_dev_in:       dState=%p\n", dState);    
    fprintf(stderr, "ring_dev_in:          msg=%p\n", msg);    

    
    fprintf(stderr, "\nIN : %u\n\n", msg->count); 
    dState->count = msg->count;
    
    fprintf(stderr, "ring_dev_in:     end\n");    
    
    return true;
}

void dev_out_send_handler(
    const graph_props *gProps,
    const dev_props *dProps,
    dev_state *dState,
    token_msg *msg
){
    fprintf(stderr, "ring_dev_out:     begin\n");    
    fprintf(stderr, "ring_dev_out:       gProps=%p\n", gProps);    
    fprintf(stderr, "ring_dev_out:       dProps=%p\n", dProps);    
    fprintf(stderr, "ring_dev_out:       dState=%p\n", dState);    
    fprintf(stderr, "ring_dev_out:          msg=%p\n", msg);    

    
    msg->count = dState->count+gProps->increment;
    dState->count=0;

    fprintf(stderr, "ring_dev_out:     end\n");    

}

InputPortVTable INPUT_VTABLES_dev[INPUT_COUNT_dev]={
    {
        (receive_handler_t)dev_in_receive_handler,
        sizeof(packet_t)+sizeof(token_msg)
    }
};

OutputPortVTable OUTPUT_VTABLES_dev[OUTPUT_COUNT_dev]={
    {
        (send_handler_t)dev_out_send_handler,
        sizeof(packet_t)+sizeof(token_msg)
    }
};

DeviceTypeVTable DEVICE_TYPE_VTABLES[DEVICE_TYPE_COUNT] = {
    {
        (ready_to_send_handler_t)dev_ready_to_send_handler,
        OUTPUT_COUNT_dev,
        OUTPUT_VTABLES_dev,
        INPUT_COUNT_dev,
        INPUT_VTABLES_dev,
        (compute_handler_t)0 // No compute handler
    }
};


///////////////////////////////////////////
// Graph instance

const unsigned DEVICE_INSTANCE_COUNT=2;

// A graph of two devices. Both devices live on
// the same thread with address zero, and have
// device indexes 0 and 1.

dev_state dev0_state{
    1
};
address_t dev0_out_addresses[1]={
    {
        0, // thread 0
        1, // device index 1
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
        0, // thread 0
        0, // device index 0
        INPUT_INDEX_dev_in
    }
};
OutputPortTargets dev1_targets[1]={
    {
        1,
        dev1_out_addresses
    }
};

DeviceContext DEVICE_INSTANCE_CONTEXTS[2]={
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

graph_props inst0_props{
    2 // increment by 2
};

extern PThreadContext softswitch_pthread_context{
    &inst0_props,
    DEVICE_TYPE_COUNT,
    DEVICE_TYPE_VTABLES,
    DEVICE_INSTANCE_COUNT,
    DEVICE_INSTANCE_CONTEXTS,
    0, // rtsHead
    0, // rtsTail
    0, // rtcChecked
    0  // rtcOffset
};

