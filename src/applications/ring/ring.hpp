#ifndef application_ring_hpp
#define application_ring_hpp

#include <cstdint>
#include <cstdio>

#include "softswitch.hpp"

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

extern DeviceTypeVTable DEVICE_TYPE_VTABLES[];

#endif
