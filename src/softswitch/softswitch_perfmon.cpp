#include "softswitch.hpp"
#include "tinsel_api.hpp"
#include <cstdarg>

#ifdef SOFTSWITCH_ENABLE_PROFILE

typedef unsigned int size_t;
extern "C" int vsnprintf (char * s, size_t n, const char * format, va_list arg );
extern "C" unsigned deltaCycles(unsigned tstart, unsigned tend); 
extern "C" void appende_vprintf(int &left, char *&dst, const char *msg, va_list v);
extern "C" void append_printf(int &left, char *&dst, const char *msg, ...);
extern "C" void softswitch_handler_export_profiler_data_impl(const PThreadContext *ctxt);

//! computes the delta between two cycle readings
extern "C" unsigned deltaCycles(unsigned tstart, unsigned tend) {
    if(tend > tstart)
        return tend - tstart;
    else
        return (tend - tstart) + 0x100000000;
}

extern "C" void append_profile_vprintf(int &left, char *&dst, const char *msg, va_list v)
{
    int done=vsnprintf(dst, left, msg, v);
    if(done>=left){
      done=left-1;
    }
    dst+=done;
    left-=done;
}

extern "C" void append_profile_printf(int &left, char *&dst, const char *msg, ...)
{
    va_list v;
    va_start(v,msg);
    append_profile_vprintf(left, dst, msg, v);
    va_end(v);
}

//! Exports the softswitch profiled data to host
extern "C" void softswitch_handler_export_profiler_data_impl(const PThreadContext *ctxt)
{
    assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

    char buffer[256];
    int left=sizeof(buffer)-3;
    char *dst=buffer;
    
    for(int i=0; i<255; i++) { buffer[i] = 0; } //To-Do replace with something better

    const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;
    
    unsigned send_handler = ctxt->sendHandler_cnt;
    unsigned send_overhead = ctxt->sendOverhead_cnt;
    unsigned recv_handler = ctxt->recvHandler_cnt;
    unsigned recv_overhead = ctxt->recvOverhead_cnt;
    unsigned idle = ctxt->idle_cnt;

    //Outputs the performance counters in the following format
    // [thread id], device, initialisation count, blocked count, send overhead count, send handler count, recv overhead count, recv handler count, idle count
    append_profile_printf(left, dst, "[%08x], %u, %u, %u, %u, %u, %u\n", tinsel_myId(), deltaCycles(ctxt->startCycle_val, ctxt->initDoneCycle_val), ctxt->waitToSend_cnt + ctxt->waitToRecv_cnt, send_overhead - send_handler, send_handler, recv_overhead - recv_handler, recv_handler, idle); 

    tinsel_puts(buffer);
}

#endif
