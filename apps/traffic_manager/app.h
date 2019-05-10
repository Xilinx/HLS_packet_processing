/*
Copyright (c) 2016-2018, Xilinx, Inc.
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef APPH
#define APPH

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "hls_math.h"

#define BYTESPERCYCLE 8
typedef ap_axiu<8*BYTESPERCYCLE,1,1,1> StreamType;
const static int BUFFERCOUNT = 16;
const static int CATEGORYCOUNT = 8;
typedef ap_int<BitWidth<BUFFERCOUNT>::Value> bufferIDT;
typedef ap_uint<UnsignedBitWidth<BUFFERCOUNT>::Value> indexT;
typedef ap_uint<UnsignedBitWidth<2*BUFFERCOUNT>::Value> orderT; // What is this bound?  I think it's probably bigger than 2*BUFFERCOUNT

void process_packet(hls::stream<StreamType> &input,    // input
                    hls::stream<bufferIDT> &completed, // input
                    hls::stream<bufferIDT> &output,    // output
                    hls::stream<short> &outputLength,    // output
                    //Packet buffer_storage[BUFFERCOUNT] // Each buffer is 2Kbytes
                    ap_uint<8*BYTESPERCYCLE> buffer_storage[2048/BYTESPERCYCLE][BUFFERCOUNT] // Each buffer is 2Kbytes, assuming 1500 byte MTU.
                    );

void priority_queue_manager(hls::stream<short> &input_length_stream, // input
                            hls::stream<ap_uint<6> > &diffserv_stream, // input
                            hls::stream<bufferIDT> &buffer_id_stream, // output
                            hls::stream<bufferIDT> &completed, // input
                            hls::stream<bufferIDT> &output,    // output
                            hls::stream<short> &outputLength    // output
                            );
extern bool TEST_generate_output;

#endif
