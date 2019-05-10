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
#include "eth_interface.h"
#include "ip.hpp"


int main(int argc, char* argv[])
{
    ap_uint<48> macAddress = 0x010203040506;
    ap_uint<32> ipAddress = 0x11121314;

    ap_uint<48> serverMacAddress = 0x212223242526;
    ap_uint<32> serverIP = 0x31323334;
    int serverPort = 1884;
    int size = 64;

    ap_uint<32> *inBuf = (ap_uint<32> *)malloc(sizeof(ap_uint<32>) * 4096);
    int inLen = 0;
    ap_uint<32> *outBuf = (ap_uint<32> *)malloc(sizeof(ap_uint<32>) * 4096);
    int outLen = 0;
    STATS stats;
    unsigned int sensor;
    // Try to send something.
    process_packet(false, macAddress, ipAddress, inBuf, &inLen,
                   0, serverIP, serverPort, 1, 0, &sensor,
                   1, size, outBuf, &outLen, false, true, stats);

    // Result should be arp request
    Packet p;
    arp_hdr<Packet> ah(p);
    ethernet_hdr<arp_hdr<Packet> > arp_request(ah);
    arp_request.deserialize(outBuf, outLen);
    assert(arp_request.etherType.get() == ethernet_hdr<arp_hdr<Packet> >::ethernet_etherType::ARP);
    assert(arp_request.p.op.get() == arp_hdr<Packet>::REQUEST);
    assert(arp_request.p.pdst.get() == serverIP);
    
    std::cout << "Request from " << std::hex << 
        arp_request.p.hwsrc.get() << " " <<
        arp_request.p.psrc.get() << "\n";
    hexdump_ethernet_frame<4>(outBuf, outLen);
    
    // Generate an ARP reply
    Packet p2;
    arp_hdr<Packet> ah2(p2);
    ethernet_hdr<arp_hdr<Packet> > arp_reply(ah2);
    arp_reply.destinationMAC.set(arp_request.sourceMAC.get());
    arp_reply.sourceMAC.set(serverMacAddress);
    arp_reply.etherType.set(ethernet_hdr<arp_hdr<Packet> >::ethernet_etherType::ARP);
    arp_reply.p.op.set(arp_hdr<Packet>::REPLY);
    arp_reply.p.hwsrc.set(serverMacAddress);
    arp_reply.p.psrc.set(serverIP);
    arp_reply.p.hwdst.set(arp_request.p.hwsrc.get());
    arp_reply.p.pdst.set(arp_request.p.psrc.get());

    arp_reply.serialize(inBuf, inLen);
    process_packet(true, macAddress, ipAddress, inBuf, &inLen,
                   0, serverIP, serverPort, 1, 0, &sensor,
                   2, size, outBuf, &outLen, false, true, stats);
    assert(outLen == 0);
    process_packet(false, macAddress, ipAddress, inBuf, &inLen,
                   0, serverIP, serverPort, 1, 0, &sensor,
                   2, size, outBuf, &outLen, false, true, stats);
     Packet output_p;
    ipv4_hdr<Packet>  output_h(output_p);
    ethernet_hdr<ipv4_hdr<Packet> > output(output_h);
    output.deserialize(outBuf, outLen);

    assert(outLen > 0);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    assert(output.etherType.get() == ethernet_hdr<arp_hdr<Packet> >::ethernet_etherType::IPV4);

    for(int i = 1; i < 10; i++) {
        process_packet(false, macAddress, ipAddress, inBuf, &inLen,
                       0, serverIP, serverPort, 1, 0, &sensor,
                       20, size, outBuf, &outLen, false, true, stats);
        Packet output_p;
        ipv4_hdr<Packet>  output_h(output_p);
        ethernet_hdr<ipv4_hdr<Packet> > output(output_h);
        output.deserialize(outBuf, outLen);

        hexdump_ethernet_frame<4>(outBuf, outLen);
        assert(output.etherType.get() == ethernet_hdr<arp_hdr<Packet> >::ethernet_etherType::IPV4);
    }
}
