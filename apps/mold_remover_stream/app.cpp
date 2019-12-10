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

void process_packet(ap_uint<48> macAddresst, ap_uint<32> ipAddress, int port,
                    hls::stream<StreamType> &input,
                    hls::stream<StreamType> &output,
                    hls::stream<ap_uint<32> > &outputMeta) {
    MACAddressT macAddress = macAddresst;
#pragma HLS inline all recursive
    // Define the structure of the packets we're interested in.  Generally speaking we
    // have parallel access to the 'header' portion (the values are stored in registers),
    // and sequential access to the 'packet' portion (the values are stored in BRAM).

    auto reader = make_reader(input);
    auto writer = make_writer(output);

    // Read the headers from the input stream.
    ethernet::header eh;
    reader.get(eh);
    ipv4::header ih;
    reader.get(ih);
    ipv4::udp_header uh;
    reader.get(uh);

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
        mold64::header mh;
        reader.get(mh);
    mold_message_loop:
        // Iterate over each mold message, starting at offset 0 after the mold header.
        for(int i = 0; i < mh.get<messageCount>(); i++) {
            // Extract a mold message from the 'packet' portion
            mold64::message_header mold_message;
            reader.get(mold_message);

            // Look at the mold message to figure out how long the itch message is.
            int length = mold_message.get<messageLength>();

            // Extract the itch message and do something with it.
            // "mold_message.p" is the 'rest' of the packet after the mold message header
            // Parsing a packet is a constant time operation and the data is not moved.
            // Getting the fields of a 'parsed' header in this case is sequential, since it
            // was originally deserialized into a Packet object.
            itch::header itch_message;
            //auto itch_message = itch::header::contains(message);
            //itch_message.extend(length);
            reader.get(itch_message);
            //std::cout << "message:" << (char)itch_message.get<messageType>() << "\n";
            if(itch_message.get<messageType>() == 'R') {
                itch::directory_header m;
                // reader.read_rest();//
                //reader.get(m);
                //auto itch_directory_message = parse_itch_directory_hdr(message);
                // itch_directory_message.serialize(output);
                outputMeta << 1;
            } else if(itch_message.get<messageType>() == 'A') {
                itch::add_order_header m;
                //reader.read_rest();//
                reader.get(m);
                //auto itch_add_order_message = parse_itch_add_order_hdr(message);
                // itch_add_order_message.serialize(output);
                outputMeta << 2;
            }
        }
    } else {
        // Dump the remaining packet
        reader.read_rest();
    }
}

