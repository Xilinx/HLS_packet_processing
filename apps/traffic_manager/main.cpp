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
//#include "ip.hpp"

//long long destMAC = 0x111213141516;
//long long destIP = 0x01020304;
//int destPort = 18000;

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
template<int N>
void dumpDataBeats(hls::stream<ap_int<N> > &stream) {
    std::cout << std::hex;
    while(!stream.empty()) {
        ap_int<N> t = stream.read();
        std::cout << t << "\n";
    }
    std::cout << std::dec;
}
template<>
void dumpDataBeats<short>(hls::stream<short > &stream) {
    std::cout << std::hex;
    while(!stream.empty()) {
        ap_uint<16> t = stream.read();
        std::cout << t << "\n";
    }
    std::cout << std::dec;
}

void test_source(hls::stream<StreamType> &input) {
    int len1 = 64;                                                                                                                          
    unsigned char packet1[] =
    {
        /*00000*/  0x11,0x12,0x13,0x14,0x15,0x16,0x00,0x00,
        /*00008*/  0x00,0x00,0x00,0x00,0x08,0x00,0x45,0x00,
        /*00010*/  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,
        /*00018*/  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,
        /*00020*/  0x03,0x04,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,
        /*00028*/  0xF6,0xF7,0xF1,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00030*/  0xF0,0xF0,0xF2,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00038*/  0xF0,0xF0,0xF3,0xF0,0xDE,0xAD,0xBE,0xEF,
    };
    
    {
        ap_uint<64>* p = (ap_uint<64>*)packet1;
        int len = len1;
        for(int i = 0; i < len/8; i++) {
            StreamType t;
            t.data = *(p++);
            t.keep = -1;
            t.strb = -1;
            t.last = 0;
            if(i == len/8-1) t.last = 1;
            input << t;
        }
    }

        // eh.clear();
    // eh.destinationMAC.set(destMAC);
    // eh.etherType.set( ethernet::ethernet_etherType::IPV4);
    // ih.protocol.set(ipv4::ipv4_protocol::UDP);
    // ih.version.set(0x45);
    // ih.destination.set(destIP);
    // ih.diffserv.set(64);
    // eh.extend(64);
    // dumpAsCArray<8>(eh);
    // eh.serialize(input);

    for(int i = 0; i < 64; i++) {
    int len2 = 500;
    unsigned char packet2[500] =
    {
        /*00000*/  0x11,0x12,0x13,0x14,0x15,0x16,0x00,0x00,
        /*00008*/  0x00,0x00,0x00,0x00,0x08,0x00,0x45,0x40,
        /*00010*/  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,
        /*00018*/  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,
        /*00020*/  0x03,0x04,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00028*/  0xF6,0xF7,0xF1,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00030*/  0xF0,0xF0,0xF2,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00038*/  0xF0,0xF0,0xF3,0xF0,0xDE,0xAD,0xBE,0xEF,
    };

    packet2[15] = (i%4) << 5; // Generate some different categories.
        {
            ap_uint<64>* p = (ap_uint<64>*)packet2;
        int len = len2;
        for(int i = 0; i < len/8; i++) {
            StreamType t;
            t.data = *(p++);
            t.keep = -1;
            t.strb = -1;
            t.last = 0;
            if(i == len/8-1) t.last = 1;
            input << t;
        }
    }
    }
}
void test_sink(hls::stream<StreamType> &output) {
    dumpDataBeats(output);
}

#ifdef MAIN
static ap_uint<8*BYTESPERCYCLE> buffer_storage[2048/BYTESPERCYCLE][BUFFERCOUNT];
//Packet buffer_storage[BUFFERCOUNT];
int main(int argc, char* argv[])
{
	int result;
    hls::stream<StreamType> input;
    hls::stream<bufferIDT> completed;
    hls::stream<bufferIDT> output;
    hls::stream<short> outputLength;

    // Packet p;
    // ipv4_hdr<Packet> ih(p);
    // ethernet_hdr<ipv4_hdr<Packet> > eh(ih);
    // eh.destinationMAC.set(destMAC);
    // eh.etherType.set( ethernet::ethernet_etherType::IPV4);
    // ih.protocol.set(ipv4::ipv4_protocol::UDP);
    // ih.version.set(0x45);
    // ih.destination.set(destIP);
    // ih.diffserv.set(0);
    // eh.extend(64);
    // dumpAsCArray<8>(eh);
    // eh.serialize(input);

    int len1 = 64;                                                                                                                          
    unsigned char packet1[] =
    {
        /*00000*/  0x11,0x12,0x13,0x14,0x15,0x16,0x00,0x00,
        /*00008*/  0x00,0x00,0x00,0x00,0x08,0x00,0x45,0x00,
        /*00010*/  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,
        /*00018*/  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,
        /*00020*/  0x03,0x04,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,
        /*00028*/  0xF6,0xF7,0xF1,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00030*/  0xF0,0xF0,0xF2,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00038*/  0xF0,0xF0,0xF3,0xF0,0xDE,0xAD,0xBE,0xEF,
    };
    
    {
        ap_uint<64>* p = (ap_uint<64>*)packet1;
        int len = len1;
        for(int i = 0; i < len/8; i++) {
            StreamType t;
            t.data = *(p++);
            t.keep = -1;
            t.strb = -1;
            t.last = 0;
            if(i == len/8-1) t.last = 1;
            input << t;
        }
    }
    
    process_packet(input, completed, output, outputLength, buffer_storage);
    std::cout << "output:";
    dumpDataBeats(output);
    

    // eh.clear();
    // eh.destinationMAC.set(destMAC);
    // eh.etherType.set( ethernet::ethernet_etherType::IPV4);
    // ih.protocol.set(ipv4::ipv4_protocol::UDP);
    // ih.version.set(0x45);
    // ih.destination.set(destIP);
    // ih.diffserv.set(64);
    // eh.extend(64);
    // dumpAsCArray<8>(eh);
    // eh.serialize(input);

    for(int i = 0; i < 64; i++) {
    int len2 = 500;
    unsigned char packet2[500] =
    {
        /*00000*/  0x11,0x12,0x13,0x14,0x15,0x16,0x00,0x00,
        /*00008*/  0x00,0x00,0x00,0x00,0x08,0x00,0x45,0x40,
        /*00010*/  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,
        /*00018*/  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,
        /*00020*/  0x03,0x04,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00028*/  0xF6,0xF7,0xF1,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00030*/  0xF0,0xF0,0xF2,0xF0,0xF0,0xF0,0xF0,0xF0,
        /*00038*/  0xF0,0xF0,0xF3,0xF0,0xDE,0xAD,0xBE,0xEF,
    };

    packet2[15] = (i%4) << 5; // Generate some different categories.
        {
            ap_uint<64>* p = (ap_uint<64>*)packet2;
        int len = len2;
        for(int i = 0; i < len/8; i++) {
            StreamType t;
            t.data = *(p++);
            t.keep = -1;
            t.strb = -1;
            t.last = 0;
            if(i == len/8-1) t.last = 1;
            input << t;
        }
    }

    process_packet(input, completed, output, outputLength, buffer_storage);
    std::cout << "output:";
    dumpDataBeats(output);
    }
    std::cout << "outputLength:";
    dumpDataBeats(outputLength);

    return 0;
}
#endif
