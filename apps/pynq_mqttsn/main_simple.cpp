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
#include "ap_int.h"
#include "app.h"

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
    int i = 0;
    ap_uint<32> destIP = 0x31323334;
    int destPort = 1884;
    ap_uint<16> topicID = 0; int qos = 0;
    int size; int outLen; bool reset;

    ap_uint<32> *inBuf = (ap_uint<32> *)sds_alloc(sizeof(ap_uint<32>) * 4096);
    ap_uint<32> *outBuf = (ap_uint<32> *)sds_alloc(sizeof(ap_uint<32>) * 4096);
    float message = 0.0;
    bool messageValid = true;

    reset = false;
    b = false; // No packet;
    int count = 100;
    size = 100;
    STATS stats = {};
    process_packet(b, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, 1, size, outBuf, &outLen, reset, true, stats);
    std::cout << "Sent: " << stats.packets_sent << "\n"
              << "Recv: " << stats.packets_received << "\n"
              << "ArpsSent: " << stats.arps_sent << "\n"
              << "ArpsRecv: " << stats.arps_received << "\n"
              << "PubsSent: " << stats.publishes_sent << "\n"
              << "DupsSent: " << stats.dups_sent << "\n"
              << "AcksSent: " << stats.acks_received << "\n"
              << "Complete: " << stats.events_completed << "\n";
    char s[4] = {1,2,3,4};
    int messageID = 0;
    qos = 0;

    // We should get an ARP request.
    dumpDataBeats(outBuf, outLen);
    //   hexdump_ethernet_frame<8>(outBuf, outLen);

    assert(outLen == 64);
    ap_uint<32> goldenout[64]  = {
        0xffffffff, 0x201ffff, 0x6050403, 0x1000608,
        0x4060008, 0x2010100, 0x6050403, 0x14131211,
        0x0, 0x32310000, 0x3433, 0x0, 0x0, 0x0, 0x0, 0x0, };
    for(int i=0; i < outLen/4; i++) {
        assert(outBuf[i] == goldenout[i]);
    }

    inLen = 64;
    ap_uint<32> goldenarp[64]  = {
        0x4030201, 0x22210605, 0x26252423, 0x1000608,
        0x4060008, 0x22210200, 0x26252423, 0x34333231,
        0x4030201, 0x12110605, 0x1413, 0x0, 0x0, 0x0, 0x0, 0x0, };
    for(int i=0; i < outLen/4; i++) {
        inBuf[i] = goldenarp[i];
    }
    b = true;
    process_packet(b, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);
    process_packet(false, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);
    process_packet(false, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);
    process_packet(false, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);
    process_packet(false, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);
    //   process_packet(false, macAddress, ipAddress, inBuf, &inLen, i++, destIP, destPort, topicID, qos, message, messageValid, count, size, outBuf, &outLen, reset, true, stats);

    std::cout << "Sent: " << stats.packets_sent << "\n"
              << "Recv: " << stats.packets_received << "\n"
              << "ArpsSent: " << stats.arps_sent << "\n"
              << "ArpsRecv: " << stats.arps_received << "\n"
              << "PubsSent: " << stats.publishes_sent << "\n"
              << "DupsSent: " << stats.dups_sent << "\n"
              << "AcksSent: " << stats.acks_received << "\n"
              << "Complete: " << stats.events_completed << "\n";

     ap_uint<32> publish[64]   = {
         0x24232221, 0x2012625, 0x6050403, 0x450008,
         0x2700, 0x11400000, 0x12113af2, 0x32311413,
         0x50c33433, 0x13005c07, 0xc0b0000, 0x0, 0x2e303000, 0x30, };

    dumpDataBeats(outBuf, outLen);
    for(int i=0; i < outLen/4; i++) {
        if(outBuf[i] != publish[i]) {
            std::cout << "i = " << i << " " << outBuf[i] << " " << publish[i];
        }
        //assert(outBuf[i] == publish[i]);
    }


     // We should get an MQTTSN publish
    // dumpDataBeats(outBuf, outLen);

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
    sds_free(inBuf);
    sds_free(outBuf);
    //    cleanup_ethernet();

	return 0;
}

