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

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "ip.hpp"

typedef ap_axiu<64,1,1,1> StreamType;

void process_packet(ap_uint<48> macAddress, ap_uint<32> ipAddress, int port,
                    hls::stream<StreamType> &input,
                    hls::stream<StreamType> &output,
                    hls::stream<ap_uint<32> > &outputMeta);
