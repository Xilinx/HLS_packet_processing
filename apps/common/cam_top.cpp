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

#include "cam.h"

const static int N = 8;

void top(ap_uint<32> keys[], ap_uint<8> values[N], ap_uint<32> keysout[64], ap_uint<8> valuesout[64]) {
    hls::cam<16, ap_uint<32>, ap_uint<8> > mycam;

    for(int i = 0; i < 8; i++) {
        ap_uint<32> k = rand();
        ap_uint<8> v = rand();
    }

    for(int i = 0; i < 64; i++) {
        #pragma HLS pipeline II=1
        ap_uint<32> k = keysout[i];
        ap_uint<8> v;
        bool b = mycam.get(k,v);
        valuesout[i] = v;
    }
}

