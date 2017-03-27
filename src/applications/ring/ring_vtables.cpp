#include "ring.hpp"

///////////////////////////////////////////////
// Graph type stuff

uint32_t dev_ready_to_send_handler(
    const graph_props *gProps,
    const dev_props *dProps,
    const dev_state *dState
){
    return dState->count ? OUTPUT_FLAG_dev_out : 0;
}

void dev_in_receive_handler(
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
}

bool dev_out_send_handler(
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

    fprintf(stderr, "ring_dev_out:       count=%u, inc=%u\n", dState->count, gProps->increment);
    msg->count = dState->count+gProps->increment;
    dState->count=0;
    fprintf(stderr, "\nOUT : %u\n\n", msg->count);

    fprintf(stderr, "ring_dev_out:     end\n");    

    return true;
}

InputPortVTable INPUT_VTABLES_dev[INPUT_COUNT_dev]={
    {
        (receive_handler_t)dev_in_receive_handler,
        sizeof(packet_t)+sizeof(token_msg),
        0,
        0,
        "in"
    }
};

OutputPortVTable OUTPUT_VTABLES_dev[OUTPUT_COUNT_dev]={
    {
        (send_handler_t)dev_out_send_handler,
        sizeof(packet_t)+sizeof(token_msg),
        "out"
    }
};

DeviceTypeVTable DEVICE_TYPE_VTABLES[DEVICE_TYPE_COUNT] = {
    {
        (ready_to_send_handler_t)dev_ready_to_send_handler,
        OUTPUT_COUNT_dev,
        OUTPUT_VTABLES_dev,
        INPUT_COUNT_dev,
        INPUT_VTABLES_dev,
        (compute_handler_t)0, // No compute handler
        "dev"
    }
};
