#include "softswitch.hpp"
#include "tinsel_api.hpp"
#include <cstdarg>

#ifdef SOFTSWITCH_ENABLE_PROFILE

typedef unsigned int size_t;
extern "C" int vsnprintf (char * s, size_t n, const char * format, va_list arg );
extern "C" uint32_t deltaCycles(uint32_t tstart, uint32_t tend); 
extern "C" void appende_vprintf(int &left, char *&dst, const char *msg, va_list v);
extern "C" void append_printf(int &left, char *&dst, const char *msg, ...);
extern "C" void softswitch_handler_export_profiler_data_impl(const PThreadContext *ctxt);

//! computes the delta between two cycle readings
extern "C" uint32_t deltaCycles(uint32_t tstart, uint32_t tend) {
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


//! Exports the performance counters to the host and sets them to zero
extern "C" void perfmon_flush_counters(PThreadContext *ctxt)
{
    assert(ctxt->currentDevice < ctxt->numDevices); // Current device must be in range

    char buffer[256];
    int left=sizeof(buffer)-3;
    char *dst=buffer;
    
    for(int i=0; i<255; i++) { buffer[i] = 0; } //To-Do replace with something better

    const DeviceContext *deviceContext = ctxt->devices+ctxt->currentDevice;
    
    uint32_t thread_cycles = ctxt->thread_cycles; 
    uint32_t blocked_cycles = ctxt->blocked_cycles; 
    uint32_t idle_cycles = ctxt->idle_cycles;
    uint32_t perfmon_cycles = ctxt->perfmon_cycles; 
    uint32_t send_cycles = ctxt->send_cycles; 
    uint32_t send_handler_cycles = ctxt->send_handler_cycles; 
    uint32_t recv_cycles = ctxt->recv_cycles; 
    uint32_t recv_handler_cycles = ctxt->recv_handler_cycles; 

                                    // tid, total, blocked, idle, perfmon, send, sendhand, recv, recvhand
    append_profile_printf(left, dst, "[%08x], %u, %u, %u, %u, %u, %u, %u, %u\n", tinsel_myId(), thread_cycles, blocked_cycles, idle_cycles, perfmon_cycles, send_cycles, send_handler_cycles, recv_cycles, recv_handler_cycles); 

    tinsel_puts(buffer);
    
    //Reset the performance counters
    ctxt->thread_cycles = 0;
    ctxt->blocked_cycles = 0;
    ctxt->idle_cycles = 0;
    ctxt->perfmon_cycles = 0;
    ctxt->send_cycles = 0;
    ctxt->send_handler_cycles = 0;
    ctxt->recv_cycles = 0;
    ctxt->recv_handler_cycles = 0;
}

#endif
