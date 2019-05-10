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

#include "app.cpp"

int main(int argc, char* argv[])
{
	int result;

    //    arg_init(argc, argv);

    MACAddressT macAddress = 0x717273040506;
    IPAddressT ipAddress = 0x11121314;
    std::cout << macAddress << " " << ipAddress << "\n";
    // macAddress(47,40) = myMac.a[0];
    // macAddress(39,32) = myMac.a[1];
    // macAddress(31,24) = myMac.a[2];
    // macAddress(23,16) = myMac.a[3];
    // macAddress(15, 8) = myMac.a[4];
    // macAddress( 7, 0) = myMac.a[5];
    //  ipAddress(31,24) = myIP.a[0];
    // ipAddress(23,16) = myIP.a[1];
    // ipAddress(15, 8) = myIP.a[2];
    // ipAddress( 7, 0) = myIP.a[3];

    bool b;
    ap_uint<32> inBuf[0x10000]; int inLen;
    int i; IPAddressT destIP; int destPort; ap_uint<16> topicID; int qos; unsigned int sensor[0x10000];
    int size; ap_uint<32> outBuf[0x10000]; int outLen; bool reset;

    reset = false;
    b = false; // No packet;
    size = 100;
    process_packet(b, macAddress, ipAddress, inBuf, &inLen, i, destIP, destPort, topicID, qos, (volatile unsigned int *)sensor, size, outBuf, &outLen, reset);
    char s[4] = {1,2,3,4};
    int messageID = 0;
    qos = 0;

    Packet p;
    ipv4_hdr<Packet> ih(p);
    destIP=0x31323334;
    destPort=1884;
    ih.destination.set(destIP);
    //    ih.dport.set(dport);
    for(int i = 0; i < 20; i++) {
        p.set<1>(i,i);
    }
    p.extend(20);
    typedef hls::cam<4, IPAddressT, MACAddressT> ArpCacheT;
    ArpCacheT arpcache;
    package_ethernet_frame(macAddress, ipAddress, ih, outBuf, outLen, arpcache);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    // We should get an ARP request.

    // stuff the arpcache, simulating the effect of an ARP response.
    MACAddressT destMAC=0x212223242526;
    arpcache.insert(destIP, destMAC);
    std::cout << arpcache << "\n";
    package_ethernet_frame(macAddress, ipAddress, ih, outBuf, outLen, arpcache);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    // We should get an MQTTSN publish.

    test_source(1, macAddress, ipAddress, destIP, destPort, s,4,1,1,0,0,100, outBuf, outLen, arpcache);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    return 0;
}
