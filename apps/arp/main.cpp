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

/*
 * This file contains a simple non-synthesizeable simulation testbench.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ap_axi_sdata.h"
#include "ap_int.h"
#include "hls_stream.h"
#include "stdint.h"
#include "app.h"

#include "ip.hpp"
#include "cam.h"

//garbage?
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

using namespace mold64;
using namespace itch;

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

long long destMAC = 0x111213141516;
long long destIP = 0x01020304;
int destPort = 18000;

int main(int argc, char *argv[]) {
    ArpCacheT arp_cache;

    hls::stream<ap_axiu<W,1,1,1> > input("input");
    hls::stream<ap_axiu<W,1,1,1> > output("output");
    hls::stream<arpcache_insert_args> arpcache_insert_start("arpcache_insert_start");
    hls::stream<arpcache_insert_return> arpcache_insert_done("arpcache_insert_done");

    // MAC and IP address of remote
    MACAddressT macAddress(0x212223242526);
    IPAddressT ipAddress(0x31323334);
    int packetCount;

    {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        ethernet_hdr<ipv4_hdr<Packet> > eh(ih);
        eh.destinationMAC.set(destMAC);
        eh.etherType.set( ethernet::ethernet_etherType::IPV4);
        ih.protocol.set(ipv4::ipv4_protocol::UDP);
        ih.destination.set(destIP);
        //    uh.dport.set(destPort);
        //mh.set<messageCount>(0);
        eh.extend(64);
        std::cout << "Ingress: " << eh << "\n";
        eh.serialize(input);

        arp_ingress((MACAddressT)destMAC, (IPAddressT)destIP, input, output, arpcache_insert_start); //, arpcache_insert_done);

        ih.deserialize(output);
        std::cout << ih << "\n";
        std::cout << p << "\n";
        assert(ih.protocol.get() == ipv4::ipv4_protocol::UDP);
        assert(ih.destination.get() == destIP);

        // Should be empty
        std::cout << arp_cache;
    }

    // Try egress of a packet not in ARP cache.  Should get ARP request
    {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        ih.protocol.set(ipv4::ipv4_protocol::UDP);
        ih.destination.set(ipAddress);
        ih.extend(64);
        std::cout << "Egress: " << ih << "\n";
        ih.serialize(input);

        arp_egress((MACAddressT)destMAC, (IPAddressT)destIP, input, output, arpcache_insert_start);//, arpcache_insert_done);

        ethernet_hdr<Packet> eh(p);
        eh.deserialize(output);
        std::cout << eh << "\n";

        assert(eh.etherType.get() == ethernet::ethernet_etherType::ARP);
    }
    MACAddressT BROADCAST_MAC(0xFFFFFFFFFFFF);	// Broadcast MAC Address
    {
        ZeroPadding p;
        arp_hdr<ZeroPadding> ah(p);
        ethernet_hdr<arp_hdr<ZeroPadding> > h(ah);
        h.destinationMAC.set(BROADCAST_MAC);
        h.sourceMAC.set(macAddress);
        h.etherType.set(ethernet::ethernet_etherType::ARP); // ARP ethertype
        h.p.op.set(arp::arp_opCode::REPLY);
        h.p.hwsrc.set(macAddress);
        h.p.psrc.set(ipAddress);
        h.p.hwdst.set(destMAC);
        h.p.pdst.set(destIP);
        h.extend(64);
        std::cout << "Ingress: " << h << "\n";
        h.serialize(input);

        arp_ingress((MACAddressT)destMAC, (IPAddressT)destIP, input, output, arpcache_insert_start);//, arpcache_insert_done);

        // Should be empty
        dumpDataBeats(input);
        dumpDataBeats(output);
        // Should contain something
        std::cout << arp_cache << "\n";
    }

    // Try egress of a packet in ARP cache.  Should get packet out
    {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        ih.protocol.set(ipv4::ipv4_protocol::UDP);
        ih.destination.set(ipAddress);
        ih.extend(64);
        std::cout << "Egress: " << ih << "\n";
        ih.serialize(input);

        arp_egress((MACAddressT)destMAC, (IPAddressT)destIP, input, output, arpcache_insert_start);//, arpcache_insert_done);

        ethernet_hdr<ipv4_hdr<Packet> > eh(ih);

        eh.deserialize(output);

        std::cout << eh << "\n";
        std::cout << ih << "\n";
        assert(eh.destinationMAC.get() == macAddress);
        assert(eh.etherType.get() == ethernet::ethernet_etherType::IPV4);
        assert(ih.protocol.get() == ipv4::ipv4_protocol::UDP);
        assert(ih.destination.get() == ipAddress);
    }

    // Try egress of broadcast packet in ARP cache.  Should get packet out
    {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        ih.protocol.set(ipv4::ipv4_protocol::UDP);
        ih.destination.set(0xFFFFFFFF);
        ih.extend(64);
        std::cout << "Egress: " << ih << "\n";
        ih.serialize(input);

        arp_egress((MACAddressT)destMAC, (IPAddressT)destIP, input, output, arpcache_insert_start);//, arpcache_insert_done);

        ethernet_hdr<ipv4_hdr<Packet> > eh(ih);

        eh.deserialize(output);

        std::cout << eh << "\n";
        std::cout << ih << "\n";
        assert(eh.destinationMAC.get() == 0xFFFFFFFFFFFF);
        assert(eh.etherType.get() == ethernet::ethernet_etherType::IPV4);
        assert(ih.protocol.get() == ipv4::ipv4_protocol::UDP);
        assert(ih.destination.get() == 0xFFFFFFFF);
    }

}
