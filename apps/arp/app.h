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

#include "ip.hpp"
#include "cam.h"

const static int W = 64;
// struct arpentry {
//     struct ipaddress ip;
//     struct ethaddress eth;
// };

typedef hls::algorithmic_cam<256, 4, IPAddressT, MACAddressT> ArpCacheT;
//typedef hls::cam<4, IPAddressT, MACAddressT> ArpCacheT;

struct arpcache_insert_args {
    ap_uint<32> ip;
    ap_uint<48> mac;
};
typedef int arpcache_insert_return;
//struct arpcache_insert_return {
//    int dummy;
//};
void arp_ingress(ap_uint<48> macAddress, ap_uint<32> ipAddress,
                 stream<ap_axiu<W,1,1,1> > &dataIn, stream<ap_axiu<W,1,1,1> > &dataOut,
                 stream<arpcache_insert_args> &arpcache_insert_start);//, stream<arpcache_insert_return> &arpcache_insert_done);
//,
//               ArpCacheT &arpcache);

void arp_egress(ap_uint<48> macAddress, ap_uint<32> ipAddress,
                stream<ap_axiu<W,1,1,1> > &dataIn, stream<ap_axiu<W,1,1,1> > &dataOut,
                stream<arpcache_insert_args> &arpcache_insert_start);//, stream<arpcache_insert_return> &arpcache_insert_done);
//,
//              ArpCacheT &arpcache);
#include "packetRecorder.h"
void Top(ap_uint<48> macAddress, ap_uint<32> ipAddress,
         hls::stream<ap_axiu<W,1,1,1>> &in,
            hls::stream<ap_axiu<W,1,1,1>> &out,
            hls::stream<arpcache_insert_args> &arpcache_insert_start,
            int &packetCount,
         ap_uint<W> data[BEATS_PER_PACKET*1024]);
