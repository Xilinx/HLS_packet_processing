#pragma once

#include "axi_stream_common.hh"
#include "message.hh"

/*
 * struct input {
 *     uint16_t values[16];
 * };
 * struct output {
 *     uint16_t value;
 * };
 */
namespace input_m {
    struct value : message::lu16 {};
}

using input = message::Struct<input_m::value>::type;
namespace output_m {
    struct value : message::lu16 {};
}
using output = message::Struct<output_m::value>::type;

const int NUM_INPUTS = 1024;

void tl(AxiStream<8>& in_stream, AxiStream<8>& output_stream);
