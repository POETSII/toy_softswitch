#ifndef application_edge_props_hpp
#define application_edge_props_hpp

#include <cstdint>
#include <cstdio>

#include "softswitch.hpp"

///////////////////////////////////////////////
// Graph type stuff

const unsigned DEVICE_TYPE_COUNT = 2;
const unsigned DEVICE_TYPE_INDEX_inner = 1;
const unsigned DEVICE_TYPE_INDEX_outer = 0;

const unsigned INPUT_COUNT_inner = 1;
const unsigned INPUT_INDEX_inner_in = 0;

const unsigned OUTPUT_COUNT_inner = 0;

const unsigned INPUT_COUNT_outer = 0;

const unsigned OUTPUT_COUNT_outer = 1;
const unsigned OUTPUT_INDEX_outer_out = 0;
const unsigned OUTPUT_FLAG_outer_out = 1<<OUTPUT_INDEX_outer_out;

struct graph_props
{};

struct tick_msg
{
};

// No properties
struct outer_props
{};

struct outer_state
{
    uint8_t ready;
};

// No properties
struct inner_props
{
    uint32_t outerCount;
};

struct inner_state
{
    uint32_t acc;
};

struct inner_in_props
{
    uint32_t mask;
};

extern DeviceTypeVTable DEVICE_TYPE_VTABLES[];

#endif
