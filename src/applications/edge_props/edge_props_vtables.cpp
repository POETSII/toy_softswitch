#include "edge_props.hpp"

#include <cstdlib>

///////////////////////////////////////////////
// Graph type stuff

uint32_t outer_ready_to_send_handler(
    const graph_props *gProps,
    const outer_props *dProps,
    const outer_state *dState
){
    fprintf(stderr, "outer/rts : dState={%d}\n", dState->ready);
    return dState->ready ? OUTPUT_FLAG_outer_out : 0;
}


bool outer_out_send_handler(
    const graph_props *gProps,
    const outer_props *dProps,
    outer_state *dState,
    tick_msg *msg
){
    assert(dState->ready);
    dState->ready=0;
    
    return true;
}

uint32_t inner_ready_to_send_handler(
    const graph_props *gProps,
    const inner_props *dProps,
    const inner_state *dState
){
    return 0;
}

void inner_in_receive_handler(
    const graph_props *gProps,
    const inner_props *dProps,
    inner_state *dState,
    const inner_in_props *eProps,
    void *eState,
    tick_msg *msg
){
    fprintf(stderr, "outer/recv : acc=%x, eProps=%x\n", dState->acc, eProps->mask);
    
    assert( (dState->acc & eProps->mask) == 0);
    dState->acc |= eProps->mask;
    
    if(dState->acc == (1<<dProps->outerCount)-1){
        fprintf(stderr, "\n\n###########################\nAll ticks received\n##########################\n\n");
        exit(0);
    }
}



InputPortVTable INPUT_VTABLES_outer[INPUT_COUNT_outer]={
};

InputPortVTable INPUT_VTABLES_inner[INPUT_COUNT_inner]={
    {
        (receive_handler_t)inner_in_receive_handler,
        sizeof(packet_t)+sizeof(tick_msg),
        sizeof(inner_in_props),
        0,
        "in"
    }
};

OutputPortVTable OUTPUT_VTABLES_outer[OUTPUT_COUNT_outer]={
    {
        (send_handler_t)outer_out_send_handler,
        sizeof(packet_t)+sizeof(tick_msg),
        "out"
    }
};

OutputPortVTable OUTPUT_VTABLES_inner[OUTPUT_COUNT_inner]={
};



////////////////////////////////////////////////////////////




DeviceTypeVTable DEVICE_TYPE_VTABLES[DEVICE_TYPE_COUNT] = {
    {
        (ready_to_send_handler_t)outer_ready_to_send_handler,
        OUTPUT_COUNT_outer,
        OUTPUT_VTABLES_outer,
        INPUT_COUNT_outer,
        INPUT_VTABLES_outer,
        (compute_handler_t)0, // No compute handler
        "outer"
    },{
        (ready_to_send_handler_t)inner_ready_to_send_handler,
        OUTPUT_COUNT_inner,
        OUTPUT_VTABLES_inner,
        INPUT_COUNT_inner,
        INPUT_VTABLES_inner,
        (compute_handler_t)0, // No compute handler
        "inner"
    }
};
