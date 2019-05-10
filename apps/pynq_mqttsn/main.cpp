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

#ifdef __SDSCC__
#include "sds_lib.h"
#else
#define sds_alloc malloc
#define sds_free free
#endif

#include "app.h"
#include "eth_interface.h"
#include "ip.hpp"

int myPort;
long long destIP;
int destPort;

#define AbortOn(c, x) if(c) {std::cout << "ERROR @ " << __FILE__ << ":" << __LINE__ << " - " << x << std::endl;  exit(-1);}

#ifdef SIM
void arg_init(int argc, char* argv[]) {
 	AbortOn(argc != 3, "Calling format is <inFile> <outFile>");
    const char *filename = argv[1];
    init_ethernet_sim(argv[1], argv[2]);
    myPort = 1024;
    destIP = 0xDEADF00D;
    destPort = 1024;
}
#else
void arg_init(int argc, char* argv[]) {
	AbortOn(argc != 4, "Calling format is <interface> <destIP> <destPort>");
	struct sockaddr_in myAddr;
	int result;
    myPort = 1024;
    char * interface = argv[1];
    in_addr_t dest_inet = inet_addr(argv[2]);
    ap_uint<32> destIP(byteSwap32(dest_inet));
	destPort = atoi(argv[3]);
    init_ethernet_raw(interface, destPort);
}
#endif

template<int N>
void dumpDataBeats(ap_uint<N> _buf[], int len) {
    const int S = N/8;
    std::cout << "len = " << std::dec << len << "\n";
    std::cout << std::hex << " = {\n";
    for(int i= 0; i < (len+S-1)/S; i++) {
        std::cout << "0x" << (unsigned int)_buf[i] << ", ";
    }
    std::cout << "};\n";
}
int main(int argc, char* argv[])
{
	int result;

    //    arg_init(argc, argv);

    ap_uint<48> macAddress = 0x010203040506;
    ap_uint<32> ipAddress = 0x11121314;
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
    int inLen;
    int i;
    ap_uint<32> destIP = 0x31323334;
    int destPort = 1884;
    ap_uint<16> topicID; int qos;
    int size; int outLen; bool reset;

    ap_uint<32> *networkIOP = (ap_uint<32> *)sds_alloc(sizeof(ap_uint<32>) * 4096);
    unsigned int *sensor = (unsigned int *)sds_alloc(sizeof(unsigned int) * 4096);

    ap_uint<32> *inBuf = networkIOP + 0x200;
    ap_uint<32> *outBuf = networkIOP;

    networkIOP[0x390] = 0; // No input packet.
    networkIOP[0x394] = 0; // No input packet.

    qos=0;
    reset = false;
    int count = 1;
    size = 100;
    STATS stats = {};
    Top(size, count, macAddress, ipAddress, destIP, destPort, topicID, qos, true, (volatile ap_uint<32> *)networkIOP, (volatile char *)sensor);

    // std::cout << "Sent: " << stats.packets_sent << "\n"
    //           << "Recv: " << stats.packets_received << "\n"
    //           << "ArpsSent: " << stats.arps_sent << "\n"
    //           << "ArpsRecv: " << stats.arps_received << "\n"
    //           << "PubsSent: " << stats.publishes_sent << "\n"
    //           << "DupsSent: " << stats.dups_sent << "\n"
    //           << "AcksSent: " << stats.acks_received << "\n"
    //           << "Complete: " << stats.events_completed << "\n";
    char s[4] = {1,2,3,4};
    int messageID = 0;
    qos = 0;

    assert(networkIOP[0x190] != 0);
    networkIOP[0x190] = 0; // Acknowledge the output packet.
    outLen = networkIOP[0x194];

    // We should get an ARP request.
    std::cout << "Expecting ARP request:\n";
    dumpDataBeats(outBuf, outLen);
    hexdump_ethernet_frame<8>(outBuf, outLen);
    Packet p;
    arp_hdr<Packet> ah(p);
    ethernet_hdr<arp_hdr<Packet> > h(ah);

    h.deserialize(outBuf, outLen);
    assert(outLen > 0);
    assert(h.etherType.get() == ethernet::ethernet_etherType::ARP);

    // Convert the ARP request into an ARP reply.
    ah.pdst.set(ah.psrc.get());
    ah.hwdst.set(ah.hwsrc.get());
    ah.psrc.set(destIP);
    ah.hwsrc.set(0x212223242526);
    ah.op.set(arp::arp_opCode::REPLY);
    h.destinationMAC.set(h.sourceMAC.get());
    h.sourceMAC.set(0x212223242526);
    h.serialize(inBuf, inLen);
    b = true;

    std::cout << "Receiving ARP reply:\n";
    dumpDataBeats(inBuf, inLen);
    hexdump_ethernet_frame<8>(inBuf, inLen);
    networkIOP[0x390] = 1; // Input packet.
    networkIOP[0x394] = inLen; 

    Top(size, 1, macAddress, ipAddress, destIP, destPort, topicID, qos, true, (volatile ap_uint<32> *)networkIOP, (volatile char *)sensor);

    assert(networkIOP[0x190] != 0);
    networkIOP[0x190] = 0; // Acknowledge the output packet.
    outLen = networkIOP[0x194];

    // We should get an MQTTSN publish
    std::cout << "Expecting MQTTSN publish:\n";
    dumpDataBeats(outBuf, outLen);
    hexdump_ethernet_frame<8>(outBuf, outLen);
    Packet p2;
    ethernet_hdr<Packet> h2(p2);
    h2.deserialize(outBuf, outLen);

    assert(outLen > 0);
    assert(h2.etherType.get() == ethernet::ethernet_etherType::IPV4);

    /*
    Packet p;
    ipv4_hdr<Packet> ih(p);
    destIP=0x31323334;
    destPort=1884;
    ih.destination.set(destIP);
    //    ih.dport.set(dport);
    for(int i = 0; i < 20; i++) {
        p.set<1>(i,i);
    }
    typedef hls::cam<4, MacLookupKeyT, MacLookupValueT> ArpCacheT;
    ArpCacheT arpcache;
    package_ethernet_frame(macAddress, ipAddress, ih, outBuf, &outLen, arpcache);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    // We should get an ARP request.

    // stuff the arpcache, simulating the effect of an ARP response.
    ap_uint<48> destMAC=0x212223242526;
    arpcache.insert(destIP, destMAC);

    package_ethernet_frame(macAddress, ipAddress, ih, outBuf, &outLen, arpcache);
    hexdump_ethernet_frame<4>(outBuf, outLen);
    // We should get an MQTTSN publish.
    */

    //  static ArpCacheT arpcache;
    //testsource(1,macAddress,ipAddress, destIP, destPort, s, 4, messageID, topicID, qos, dup, size, outBuf, tLen, arpcache);
    // struct timespec start_time, end_time;
    // for(int size = 100; size < 1500; size += 100) {
    //     for(int i = 0 ; i < 3; i++) {
    //         int count = 10000*(i+1);
    //         clock_gettime(CLOCK_REALTIME, &start_time);
    //         Top(size, count, macAddress, ipAddress, destIP, destPort);//, arpiptable, arpethtable);
    //         clock_gettime(CLOCK_REALTIME, &end_time);

    //         double accum = double( end_time.tv_sec - start_time.tv_sec )
    //             + double( end_time.tv_nsec - start_time.tv_nsec )
    //             / 1000000000.0;
    //         std::cout << count << " " << size << "-byte packets in " << accum << " seconds.\n";
    //         std::cout << count/accum << " packets per second.\n";
    //         std::cout << ((count/accum)*size*8)/1000000 << " mbps\n";
    //     }
    // }

    // for(int i = 0; i < 16; i++) {
    //     for(int j = 0; j < 6; j++) {
    //         std::cout << std::hex << std::setw(2) << (int)arpethtable[i].a[j] << ":";
    //     }
    //     for(int j = 0; j < 4; j++) {
    //         std::cout << std::dec << (int)arpiptable[i].a[j] << ".";
    //     }
    //     std::cout << "\n";
    // }

	//close(socketHandle);
    sds_free(networkIOP);
    sds_free(sensor);
    cleanup_ethernet();

	return 0;
}
