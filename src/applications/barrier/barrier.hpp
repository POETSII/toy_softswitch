#ifndef application_barrier_hpp
#define application_barrier_hpp

#include <cstdint>
#include <cstdio>

#include "softswitch.hpp"

///////////////////////////////////////////////
// Graph type stuff

const unsigned DEVICE_TYPE_COUNT = 2;
const unsigned DEVICE_TYPE_INDEX_dev = 0;
const unsigned DEVICE_TYPE_INDEX_halt = 1;

const unsigned INPUT_COUNT_dev = 1;
const unsigned INPUT_INDEX_dev_in = 0;

const unsigned OUTPUT_COUNT_dev = 2;
const unsigned OUTPUT_INDEX_dev_out = 0;
const unsigned OUTPUT_FLAG_dev_out = 1<<OUTPUT_INDEX_dev_out;
const unsigned OUTPUT_INDEX_halt_out = 1;
const unsigned OUTPUT_FLAG_halt_out = 1<<OUTPUT_INDEX_halt_out;

const unsigned INPUT_COUNT_halt = 1;
const unsigned INPUT_INDEX_halt_in = 0;

const unsigned OUTPUT_COUNT_halt = 0;


struct graph_props
{
    uint32_t maxTime;
    uint32_t devCount;
};

struct tick_msg
{
    uint32_t t;
};

struct done_msg
{
};

// No properties
struct dev_props
{
};

struct dev_state
{
    uint32_t t;
    uint32_t seenNow;
    uint32_t seenNext;
};

// No properties
struct halt_props
{
};

struct halt_state
{
    uint32_t seen;
};

extern DeviceTypeVTable DEVICE_TYPE_VTABLES[];

#endif
