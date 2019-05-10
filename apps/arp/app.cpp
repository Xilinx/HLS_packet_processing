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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "app.h"
#include "ip.hpp"
#include "cam.h"

using namespace mqttsn;
using namespace arp;

const MACAddressT BROADCAST_MAC(0xFFFFFFFFFFFF);	// Broadcast MAC Address
const IPAddressT BROADCAST_IP(0xFFFFFFFF);	// Broadcast IP Address



void arp_egress(ap_uint<48> macAddress, ap_uint<32> ipAddress, stream<ap_axiu<W,1,1,1> > &dataIn, stream<ap_axiu<W,1,1,1> > &dataOut,
                stream<arpcache_insert_args> &arpcache_insert_start) {//, stream<arpcache_insert_return> &arpcache_insert_done/* ArpCacheT &arpcache*/) {
    //#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE s_axilite port=macAddress clock=AXIclock
#pragma HLS INTERFACE s_axilite port=ipAddress clock=AXIclock
#pragma HLS INTERFACE axis port=dataIn
#pragma HLS INTERFACE axis port=dataOut
#pragma HLS INTERFACE axis port=arpcache_insert_start

    static ArpCacheT arpcache;
        //#pragma HLS inline all recursive
    struct arpcache_insert_args arg;
    if(!arpcache_insert_start.empty()) {
        arg = arpcache_insert_start.read();

        arpcache.insert(arg.ip, arg.mac);

        //        struct arpcache_insert_return ret;
        //arpcache_insert_done.write(ret);
    }
    arpcache.sweep();

    auto reader = make_reader(dataIn);
    auto writer = make_writer(dataOut);

    ipv4::header ih = reader.get<ipv4::header>();

    // Below is boilerplate
    MACAddressT destMac;
    bool hit;
    IPAddressT destIP;
    destIP = ih.get<ipv4::destination>();
    //     if ((dstIpAddress & regSubNetMask) == (regDefaultGateway & regSubNetMask) || dstIpAddress == 0xFFFFFFFF)
        //     // Address is on local subnet
        //     // Perform an ARP cache lookup on the destination.
        //     arpTableOut.write(dstIpAddress);
        // else
        //     // Address is not on local subnet
        //     // Perform an ARP cache lookup on the default gateway.
        //     arpTableOut.write(regDefaultGateway);

    if (destIP == BROADCAST_IP) {	// If the destination is the IP broadcast address
        hit = true;
        destMac = BROADCAST_MAC;
    } else {
        IPAddressT destIP2(destIP);
        hit = arpcache.get(destIP2, destMac);
    }
    // std::cout << destIP << " -> " << destMac << "\n";

    // If the result is not found then fire a MAC request
    if (!hit) {
        ZeroPadding p;
        arp_hdr<ZeroPadding> ah(p);
        ethernet_hdr<arp_hdr<ZeroPadding> > h(ah);

        // send ARP request
        h.destinationMAC.set(BROADCAST_MAC);
        h.sourceMAC.set(macAddress);
        h.etherType.set(ethernet::ethernet_etherType::ARP); // ARP ethertype
        h.p.op.set(arp_opCode::REQUEST);
        h.p.hwsrc.set(macAddress);
        h.p.psrc.set(ipAddress);
        h.p.hwdst.set(0); // empty
        h.p.pdst.set(destIP);
        h.extend(64);
        // for(int i = 0; i < h.p.data_length(); i++) {
        //     std::cout << std::hex << h.p.get<1>(i) << " ";
        // }
        // std::cout << "\n";
        // for(int i = 0; i < p.data_length(); i++) {
        //     std::cout << std::hex << p.get<1>(i) << " ";
        // }
        // std::cout << "\n";
        h.serialize(dataOut);
        reader.read_rest();
        //stats.arps_sent++;
        //        hexdump_ethernet_frame<4>(buf, len);
    } else {
        // Add ethernet header
        // ethernet_hdr<ipv4_hdr<Payload> >  eh(ih);
        // eh.destinationMAC.set(destMac);
        // eh.sourceMAC.set(macAddress);
        // eh.etherType.set(ethernet::ethernet_etherType::IPV4);
        // eh.serialize(buf, len);
        // Copy workaround here to avoid HLS bug.
        ethernet::header eh;
        eh.set<ethernet::destinationMAC>(destMac);
        eh.set<ethernet::sourceMAC>(macAddress);
        eh.set<ethernet::etherType>(ethernet::ethernet_etherType::IPV4);
        writer.put(eh);
        writer.put(ih);
        writer.put_rest(reader);
        // hexdump_ethernet_frame<4>(buf, len);
    }
    //    hexdump_ethernet_frame<4>(outBuf, len);
}


void arp_ingress(ap_uint<48> macAddress, ap_uint<32> ipAddress, stream<ap_axiu<W,1,1,1> > &dataIn, stream<ap_axiu<W,1,1,1> > &dataOut,
                 stream<arpcache_insert_args> &arpcache_insert_start) {//, stream<arpcache_insert_return> &arpcache_insert_done/* ArpCacheT &arpcache*/) {
#pragma HLS inline all recursive
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE s_axilite port=macAddress clock=AXIclock
#pragma HLS INTERFACE s_axilite port=ipAddress clock=AXIclock
#pragma HLS INTERFACE axis port=dataIn
#pragma HLS INTERFACE axis port=dataOut
#pragma HLS INTERFACE axis port=arpcache_insert_start
    //#pragma HLS INTERFACE axis port=arpcache_insert_done
    auto reader = make_reader(dataIn);
    auto writer = make_writer(dataOut);

    ethernet::header eh = reader.get<ethernet::header>();

    MACAddressT destinationMAC = eh.get<ethernet::destinationMAC>();
    if(destinationMAC != macAddress &&
       destinationMAC != BROADCAST_MAC) {
#ifndef __SYNTHESIS__
        //std::cout << "Dropping invalid MAC address: " << macAddress.toString(16, false) << " " << destinationMAC.toString(16, false) << "\n";
        // hexdump_ethernet_frame<4>(buf, len);
#endif
        // HACK        return;
    }
    ap_uint<16> dmp_macType = eh.get<ethernet::etherType>();
    if (dmp_macType == ethernet::ethernet_etherType::ARP) {
        //stats.arps_received++;
        //        auto ah = parse_arp_hdr(ih);
        arp::header ah = reader.get<arp::header>();

        ap_uint<16> opCode = ah.get<op>();
        MACAddressT hwAddrSrc = ah.get<hwsrc>();
        IPAddressT protoAddrSrc = ah.get<psrc>();
        IPAddressT protoAddrDst = ah.get<pdst>();
        reader.read_rest();

#ifndef __SYNTHESIS__
        std::cout << "ARP " <<
            protoAddrDst << " " <<
            ipAddress << " " <<
            "Opcode = " << opCode << " \n";
#endif
        //We don't need to generate ARP replies in hardware.
        if ((opCode == arp_opCode::REPLY) && (protoAddrDst == ipAddress)) {
            ZeroPadding p;
            arp_hdr<ZeroPadding> ah(p);
            ethernet_hdr<arp_hdr<ZeroPadding> > h(ah);
            h.destinationMAC.set(BROADCAST_MAC);
            h.sourceMAC.set(macAddress);
            h.etherType.set(ethernet::ethernet_etherType::ARP); // ARP ethertype
            h.p.op.set(arp_opCode::REPLY);
            h.p.hwsrc.set(macAddress);
            h.p.psrc.set(ipAddress);
            h.p.hwdst.set(hwAddrSrc);
            h.p.pdst.set(protoAddrSrc);
            h.extend(64);
        }
        //    arpReplyMetaFifo.write(meta);
        struct arpcache_insert_args arg;
        arg.ip = protoAddrSrc;
        arg.mac = hwAddrSrc;

        arpcache_insert_start.write(arg);
        //struct arpcache_insert_return ret;
        //        ret = arpcache_insert_done.read();
        //        arpcache.insert(protoAddrSrc, hwAddrSrc);
#ifndef __SYNTHESIS__
        std::cout << "insert: " << hwAddrSrc << " " << protoAddrSrc << "\n";
        // std::cout << "arp cache = " << arpcache << "\n";
#endif
    }
    else {
#ifndef __SYNTHESIS__
        //std::cout << "IPv4\n";
        // << std::hex << protoAddrDst << " " << ipAddress << " " << "Opcode = " << opCode << " \n";
#endif
        //  writer.put(eh);
        writer.put_rest(reader);

    }
}

/*
#include "packetRecorder.cpp"
void Top(ap_uint<48> macAddress, ap_uint<32> ipAddress,
         hls::stream<ap_axiu<W,1,1,1>> &in,
            hls::stream<ap_axiu<W,1,1,1>> &out,
            hls::stream<arpcache_insert_args> &arpcache_insert_start,
            int &packetCount,
            ap_uint<W> data[BEATS_PER_PACKET*1024]) {
    #pragma HLS dataflow
    #pragma HLS interface ap_stable port=macAddress
    #pragma HLS interface ap_stable port=ipAddress
    hls::stream<ap_axiu<W,1,1,1> > t;
    arp_egress(macAddress, ipAddress, in, t, arpcache_insert_start);//, arpcache_insert_done);
    packetRecorder(t, out, false, packetCount, data);
}
*/


