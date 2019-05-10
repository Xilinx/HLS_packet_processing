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

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <stdio.h>

#include "app.h"
#include "ip.hpp"
//#include "eth_interface.h"


const MACAddressT BROADCAST_MAC	= 0xFFFFFFFFFFFF;	// Broadcast MAC Address
const IPAddressT BROADCAST_IP =  0xFFFFFFFF;	// Broadcast IP Address

using namespace mold64;
using namespace itch;

void process_packet_config(hls::stream<ap_uint<64> > &config,
                           hls::stream<StreamType> &inp,
                           //                           hls::stream<StreamType> &outp,
                           hls::stream<ap_uint<32> > &outputMeta) {
    static MACAddressT macAddress;
    static IPAddressT ipAddress;
    static int port;

    if(!config.empty()) {
        macAddress = config.read();
        ipAddress = config.read();
        port = config.read();
    }
    process_packet(macAddress, ipAddress, port, inp, /*outp,*/ outputMeta);
}

void process_packet(ap_uint<48> macAddress, ap_uint<32> ipAddress, int port,
                    hls::stream<StreamType> &input,
                    //                    hls::stream<StreamType> &output,
                    hls::stream<ap_uint<32> > &outputMeta) {
#pragma HLS inline all recursive
    // Define the structure of the packets we're interested in.  Generally speaking we
    // have parallel access to the 'header' portion (the values are stored in registers),
    // and sequential access to the 'packet' portion (the values are stored in BRAM).
    Packet p;
    auto mh = mold64::header::contains(p);
    auto uh = ipv4::udp_header::contains(mh);
    auto ih = ipv4::header::contains(uh);
    auto eh = ethernet::header::contains(ih);

    // Read the header and remainder from the input stream.
    eh.deserialize(input);

    // Check that the packet is something we care about.
    ap_uint<16> dmp_macType = eh.get<ethernet::etherType>();
    bool valid = true;
    MACAddressT destinationMAC = eh.get<ethernet::destinationMAC>();
    if(destinationMAC != macAddress &&
       destinationMAC != BROADCAST_MAC) {
#ifndef __SYNTHESIS__
        std::cout << "Dropping invalid MAC address: " << macAddress.toString(16, false) << " " << destinationMAC.toString(16, false) << "\n";
        // hexdump_ethernet_frame<4>(buf, len);
#endif
        valid = false;
    }
    if(eh.get<ethernet::etherType>() != ethernet::ethernet_etherType::IPV4) {
        valid = false;
    }
    if(ih.get<ipv4::protocol>() != ipv4::ipv4_protocol::UDP) {
        valid = false;
    }
    if(ih.get<ipv4::destination>() != ipAddress) {
        valid = false;
    }
    if(uh.get<ipv4::dport>() != port) {
        valid = false;
    }
    if(valid) {
        int offset = 0;
    mold_message_loop:
        // Iterate over each mold message, starting at offset 0 after the mold header.
        for(int i = 0; i < mh.get<messageCount>(); i++) {
            // Extract a mold message from the 'packet' portion
            auto next_message = skip(p, offset);
            auto mold_message = parse_mold_message_hdr(next_message);

            // Look at the mold message to figure out where the next one starts.
            offset += mold_message.get<messageLength>() + 2; // the length doesn't include the size of the length field itself.

            // Extract the itch message and do something with it.
            // "mold_message.p" is the 'rest' of the packet after the mold message header
            // Parsing a packet is a constant time operation and the data is not moved.
            // Getting the fields of a 'parsed' header in this case is sequential, since it
            // was originally deserialized into a Packet object.
            auto itch_message = parse_itch_hdr(mold_message.p);
            //std::cout << "message:" << (char)itch_message.get<messageType>() << "\n";
            if(itch_message.get<messageType>() == 'R') {
                auto itch_directory_message = parse_itch_directory_hdr(itch_message.p);
                //itch_directory_message.serialize(output);
                outputMeta << 1;
            } else if(itch_message.get<messageType>() == 'A') {
                auto itch_add_order_message = parse_itch_add_order_hdr(itch_message.p);
                //itch_add_order_message.serialize(output);
                outputMeta << 2;
            }
        }
    }
}

