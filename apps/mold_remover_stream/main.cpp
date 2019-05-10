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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <netdb.h>
#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include "app.h"
//#include "eth_interface.h"
#include "ip.hpp"

long long destMAC = 0x111213141516;
long long destIP = 0x01020304;
int destPort = 18000;

using namespace mold64;
using namespace itch;
using namespace ipv4;
using namespace ethernet;

template<typename dataBeat>
void dumpDataBeats(hls::stream<dataBeat> &stream) {
    std::cout << std::hex;
    while(!stream.empty()) {
        dataBeat t = stream.read();
        std::cout << t.data << " " << t.last << "\n";
    }
    std::cout << std::dec;
}
template<int N>
void dumpDataBeats(hls::stream<ap_uint<N> > &stream) {
    std::cout << std::hex;
    while(!stream.empty()) {
        ap_uint<N> t = stream.read();
        std::cout << t << "\n";
    }
    std::cout << std::dec;
}

int main(int argc, char* argv[])
{
	int result;
    hls::stream<StreamType> input;
    hls::stream<StreamType> output;
    hls::stream<ap_uint<32> > outputMeta;

    {
        auto mh = mold64::header::contains();
        auto uh = ipv4::udp_header::contains(mh);
        auto ih = ipv4::header::contains(uh);
        auto eh = ethernet::header::contains(ih);
        eh.set<destinationMAC>(destMAC);
        eh.set<etherType>( ethernet::ethernet_etherType::IPV4);
        ih.set<protocol>(ipv4::ipv4_protocol::UDP);
        ih.set<destination>(destIP);
        uh.set<dport>(destPort);
        mh.set<messageCount>(0);

        std::cout << mh << "\n";
        eh.serialize(input);
        process_packet(destMAC, destIP, destPort, input, output, outputMeta);
        dumpDataBeats(outputMeta);
        //        dumpDataBeats(output);
    }

    {
        auto p = itch::directory_header::contains();
        auto itch = itch::header::contains(p);
        auto mess = mold64::message_header::contains(itch);
        auto mh = mold64::header::contains(mess);
        auto uh = ipv4::udp_header::contains(mh);
        auto ih = ipv4::header::contains(uh);
        auto eh = ethernet::header::contains(ih);

        eh.set<destinationMAC>(destMAC);
        eh.set<etherType>( ethernet::ethernet_etherType::IPV4);
        ih.set<protocol>(ipv4::ipv4_protocol::UDP);
        ih.set<destination>(destIP);
        uh.set<dport>(destPort);
        mh.set<messageCount>(1);
        itch.set<messageType>('R'); // Directory message
        p.set<1>(0, 'X');
        p.set<1>(1, 'L');
        p.set<1>(2, 'N');
        p.set<1>(3, 'X');
        mess.set<messageLength>(2+itch.data_length());

        std::cout << mh << "\n";
        eh.serialize(input);
        process_packet(destMAC, destIP, destPort, input, output, outputMeta);
        std::cout << "Done processing\n";
        dumpDataBeats(outputMeta);
    //  dumpDataBeats(output);
    }

    {
        auto p = itch::add_order_header::contains();
        auto itch = itch::header::contains(p);
        auto mess = mold64::message_header::contains(itch);
        auto mh = mold64::header::contains(mess);
        auto uh = ipv4::udp_header::contains(mh);
        auto ih = ipv4::header::contains(uh);
        auto eh = ethernet::header::contains(ih);

        eh.set<destinationMAC>(destMAC);
        eh.set<etherType>( ethernet::ethernet_etherType::IPV4);
        ih.set<protocol>(ipv4::ipv4_protocol::UDP);
        ih.set<destination>(destIP);
        uh.set<dport>(destPort);
        mh.set<messageCount>(1);
        itch.set<messageType>('A'); // Add order message
        mess.set<messageLength>(2+itch.data_length());

        std::cout << mh << "\n";
        eh.serialize(input);
        process_packet(destMAC, destIP, destPort, input, output, outputMeta);
        std::cout << "Done processing\n";
        dumpDataBeats(outputMeta);
        //    dumpDataBeats(output);
    }

    return 0;
}

