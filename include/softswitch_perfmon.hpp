#ifndef softswitch_perfmon_hpp
#define softswitch_perfmon_hpp

#include "softswitch.hpp"
#include <cstdarg>

typedef unsigned int size_t;

//! computes the delta between two cycle readings
extern "C" unsigned deltaCycles(unsigned tstart, unsigned tend); 

extern "C" void append_profile_vprintf(int &left, char *&dst, const char *msg, va_list v);

extern "C" void append_profile_printf(int &left, char *&dst, const char *msg, ...);

//! Exports the softswitch profiled data to host
extern "C" void perfmon_flush_counters(PThreadContext *ctxt);

#endif
