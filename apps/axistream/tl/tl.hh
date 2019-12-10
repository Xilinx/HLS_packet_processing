/*
 *    Copyright 2017 Two Sigma Open Source, LLC
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


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
