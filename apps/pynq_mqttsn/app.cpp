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
#include <tuple>

#include "eth_interface.h"
#include "app.h"
#include "ip.hpp"
#include "cam.h"
#include "allocator.h"

using namespace mqttsn;
using namespace arp;

const MACAddressT BROADCAST_MAC	= 0xFFFFFFFFFFFF;	// Broadcast MAC Address
const IPAddressT BROADCAST_IP =  0xFFFFFFFF;	// Broadcast IP Address

//typedef hls::algorithmic_cam<256, 4, MacLookupKeyT, MacLookupValueT> ArpCacheT;
typedef hls::cam<4, IPAddressT, MACAddressT> ArpCacheT;

static STATS stats;
static bool verbose;

template<typename Payload>
void package_ethernet_frame(ap_uint<48> macAddress, ap_uint<32> ipAddress, ipv4_hdr<Payload> &ih, ap_uint<32> *buf, int &len, ArpCacheT &arpcache) {
    //#pragma HLS inline all recursive

    // Below is boilerplate
    MACAddressT destMac;
    bool hit;
    IPAddressT destIP;
    destIP = ih.destination.get();
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
        IPAddressT destIP2 = (unsigned int)destIP;
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
        h.serialize(buf, len);
        stats.arps_sent++;
        //        hexdump_ethernet_frame<4>(buf, len);
    } else {
        // Add ethernet header
        // ethernet_hdr<ipv4_hdr<Payload> >  eh(ih);
        // eh.destinationMAC.set(destMac);
        // eh.sourceMAC.set(macAddress);
        // eh.etherType.set(ethernet::ethernet_etherType::IPV4);
        // eh.serialize(buf, len);
        // Copy workaround here to avoid HLS bug.
        Packet  p;
        ipv4_hdr<Packet>   ih2(p);
        ethernet_hdr<ipv4_hdr<Packet> >  eh(ih2);
        eh.destinationMAC.set(destMac);
        eh.sourceMAC.set(macAddress);
        eh.etherType.set(ethernet::ethernet_etherType::IPV4);
    copy_workaround_loop:
        for(int i = 0; i < ih.data_length(); i++) {
#pragma HLS pipeline II=1
            ih2.template set<1>(i,ih.template get<1>(i));
        }
        ih2.extend(ih.data_length());
        eh.serialize(buf, len);
        // hexdump_ethernet_frame<4>(buf, len);
    }
    //    hexdump_ethernet_frame<4>(outBuf, len);
}


void create_mqtt_packet(mqttsn_hdr<mqttsn_publish_hdr<Packet> > & p, ap_uint<16> mID, ap_uint<16> t, int qos, bool dup) {
    p.set<type>(mqttsn_type::PUBLISH);
    ap_uint<8> f = 0;
    f[2] = 0; // set 'cleanSession'
    f(6,5) = qos; // set 'qos' field.
    if(dup) {
        f[7] = 1; // set the 'dup' field.
    }
    p.p.set<flags>(f);
    p.p.set<topicID>(t); // FIXME
    p.p.set<messageID>(mID);
    p.set<length>(p.data_length());
}
void create_connack_packet(mqttsn_hdr<mqttsn_connack_hdr<Packet> > & p, mqttsn_hdr<mqttsn_connect_hdr<Packet> > & r) {
    p.set<type>(mqttsn_type::CONNACK);
    p.p.set<returnCode>(0);
    p.set<length>(p.data_length());
}
void create_regack_packet(mqttsn_hdr<mqttsn_regack_hdr<Packet> > & p, mqttsn_hdr<mqttsn_register_hdr<Packet> > & r) {
    p.set<type>(mqttsn_type::REGACK);
    p.p.set<topicID>(r.p.get<topicID>()); // FIXME
    p.p.set<messageID>(r.p.get<messageID>());
    p.p.set<returnCode>(0);
    p.set<length>(p.data_length());
}
void create_puback_packet(mqttsn_hdr<mqttsn_puback_hdr<Packet> > & p, mqttsn_hdr<mqttsn_publish_hdr<Packet> > & r) {
    p.set<type>(mqttsn_type::PUBACK);
    p.p.set<topicID>(r.p.get<topicID>()); // FIXME
    p.p.set<messageID>(r.p.get<messageID>());
    p.p.set<returnCode>(0);
    p.set<length>(p.data_length());
}

template<typename Payload>
void create_test_packet(ap_uint<32> ipAddress, ap_uint<32> destIP, int destPort, int size, ipv4_hdr<udp_hdr<Payload> > &ih) {
    ih.lengths.set(ih.data_length());
    ih.protocol.set(ipv4_hdr<Payload>::ipv4_protocol::UDP);
    ih.source.set(ipAddress);
    ih.destination.set(destIP);
    // Compute the IP checksum last so it has information from above.
    ih.checksum.set(ih.compute_ip_checksum());

    ih.p.sport.set(50000);
    ih.p.dport.set(destPort);
    ih.p.lengths.set(ih.p.data_length());
}

void test_source(int i, ap_uint<48> macAddress, ap_uint<32> ipAddress,
                 ap_uint<32> destIP, int destPort,
                 char *s, int sLen, ap_uint<16> messageID, ap_uint<16> topicID,
                 int qos, bool dup, int size,
                 ap_uint<32> *buf, int &len, ArpCacheT &arpcache) {
#pragma HLS inline all recursive
    stats.packets_sent++;
    if(dup) stats.dups_sent++;
    Packet message;
    mqttsn_publish_hdr<Packet> mp(message);
    mqttsn_hdr<mqttsn_publish_hdr<Packet> > m(mp);
    udp_hdr<mqttsn_hdr<mqttsn_publish_hdr<Packet> > > uh(m);
    ipv4_hdr<udp_hdr<mqttsn_hdr<mqttsn_publish_hdr<Packet> > > > ih(uh);
 set_message_loop:
    for(int j = 0; j < sLen; j++) {
#pragma HLS pipeline II=1
        message.set<1>(j, s[j]);
    }
    message.extend(sLen);
    create_mqtt_packet(m, messageID, topicID, qos, dup);
    create_test_packet(ipAddress, destIP, destPort, size, ih);

    // std::cout << " length = " << ih.data_length() << " " << ih.p.data_length() << " " << ih.p.p.data_length() << " " << ih.p.p.p.data_length() << "\n"; 

    package_ethernet_frame(macAddress, ipAddress, ih, buf, len, arpcache);
}


void handle_ethernet_frame(ap_uint<48> macAddress, ap_uint<32> ipAddress, ap_uint<32> *buf, int len, ArpCacheT &arpcache,
                           MessageBuffer<std::pair<ap_uint<16>, float>, 128 > &buffer) {
#pragma HLS inline all recursive
    Packet p;
    header<Packet, 40> ih(p);
    ethernet_hdr<header<Packet, 40> > eh(ih);
    eh.deserialize(buf, len);
    MACAddressT destinationMAC = eh.destinationMAC.get();
    if(destinationMAC != macAddress &&
       destinationMAC != BROADCAST_MAC) {
#ifndef __SYNTHESIS__
        //std::cout << "Dropping invalid MAC address: " << macAddress.toString(16, false) << " " << destinationMAC.toString(16, false) << "\n";
        // hexdump_ethernet_frame<4>(buf, len);
#endif
        // HACK        return;
    }
    ap_uint<16> dmp_macType = eh.etherType.get();
    if (dmp_macType == ethernet::ethernet_etherType::ARP) {
        stats.arps_received++;
        auto ah = parse_arp_hdr(ih);
        //        parsed_arp_hdr<header<Packet, 40> > ah(ih);
        ap_uint<16> opCode = ah.get<op>();
        MACAddressT hwAddrSrc = ah.get<hwsrc>();
        IPAddressT protoAddrSrc = ah.get<psrc>();
        IPAddressT protoAddrDst = ah.get<pdst>();

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
        arpcache.insert(protoAddrSrc, hwAddrSrc);
#ifndef __SYNTHESIS__
        std::cout << "insert: " << hwAddrSrc << " " << protoAddrSrc << "\n";
        // std::cout << "arp cache = " << arpcache << "\n";
#endif
    }
    else if (dmp_macType == ethernet::ethernet_etherType::IPV4) {
#ifndef __SYNTHESIS__
        //std::cout << "IPv4\n";
        // << std::hex << protoAddrDst << " " << ipAddress << " " << "Opcode = " << opCode << " \n";
#endif
        auto h = ipv4::parse_ipv4_hdr(ih);

        if(h.get<ipv4::protocol>() == ipv4::ipv4_protocol::UDP) {
            auto uh = ipv4::parse_udp_hdr(h.p);
            if(uh.get<ipv4::sport>() == 1884) { // MQTTSN
                auto mqh = parse_mqttsn_hdr(uh.p);
                if(mqh.get<type>() == mqttsn_type::PUBLISH) {
                    auto mqpubh = parse_mqttsn_publish_hdr(mqh.p);
                    ap_uint<16> messageID = mqpubh.get<mqttsn::messageID>();
#ifndef __SYNTHESIS__
                    if(verbose) {
                        std::cout << "received MQTTSN PUBLISH ID=" << messageID << "\n";
                    }
#endif
                    if(buffer.is_allocated(messageID)) {
#ifndef __SYNTHESIS__
                        //  std::cout << "clearing ID=" << messageID << "\n";
#endif
                        buffer.clear(messageID);
                    }
                } else
                if(mqh.get<type>() == mqttsn_type::PUBACK) {
                    stats.acks_received++;
                    auto mqpubh = parse_mqttsn_puback_hdr(mqh.p);
                    ap_uint<16> messageID = mqpubh.get<mqttsn::messageID>();
#ifndef __SYNTHESIS__
                    if(verbose) {
                        std::cout << "received MQTTSN PUBACK ID=" << messageID << "\n";
                    }
                    // for(int i = 0; i < eh.data_length(); i++) {
                    //     std::cout << std::hex << eh.get<1>(i) << " ";
                    // }
                    // std::cout <<"\n";
                    // for(int i = 0; i < mqh.data_length(); i++) {
                    //     std::cout << std::hex << mqh.get<1>(i) << " ";
                    // }
                    // std::cout <<"\n";
#endif
                    if(buffer.is_allocated(messageID)) {
#ifndef __SYNTHESIS__
                        //   std::cout << "clearing ID=" << messageID << "\n";
#endif
                        stats.events_completed++;
                        buffer.clear(messageID);
                    }
                }
            }
        }
        // bool valid = check_ip_checksum(h, regIpAddress);
        // if(valid) {
        //     detect_ip_protocol(h, ICMPdataOut, ICMPexpDataOut, UDPdataOut, TCPdataOut);
        // }
    } else {
        //   arpcache.sweep();
#ifndef __SYNTHESIS__
        // std::cout << "Dropping unknown etherType " << dmp_macType.toString(16, false) << "\n";
#endif
    }
}

// The input pointer here is a virtual address to the memory of the
// appropriate IOP
std::pair<float,bool> read_sensor_hw(volatile unsigned int *sensorvirt) {
    // IOP mailbox constants
    const unsigned int MAILBOX_OFFSET = 0xF000;
    const unsigned int MAILBOX_SIZE   = 0x1000;
    const unsigned int MAILBOX_PY2IOP_CMD_OFFSET  = 0xffc;
    const unsigned int MAILBOX_PY2IOP_ADDR_OFFSET = 0xff8;
    const unsigned int MAILBOX_PY2IOP_DATA_OFFSET = 0xf00;

    // IOP mailbox commands
    const unsigned int WRITE_CMD = 0;
    const unsigned int READ_CMD  = 1;
    const unsigned int IOP_MMIO_REGSIZE = 0x10000;

    volatile unsigned int *sensorcmd = (volatile unsigned int *)(sensorvirt + (MAILBOX_OFFSET + MAILBOX_PY2IOP_CMD_OFFSET)/4);
    volatile unsigned int *sensordata = (volatile unsigned int *)(sensorvirt + MAILBOX_OFFSET/4);
    if(*(sensorcmd) != 3) {
        // If we're not in the process of doing a sensor read, then start a new read
        // and output the current value (from some previous read)
        *(sensorcmd) = 3;
    } else {
        // Otherwise wait around for a bit and see if the previous read finishes.
        int i = 0;
        while( *(sensorcmd) == 3) {
            if(i++ > 10) {
                //printf("timeout\n");
                return std::make_pair(0.0f, false);
                break;
            }
            // wait
        }
    }
    // float f = *(sensordata);
    // float f0 = f/100;
    // float f1 = f0 * 10;
    // int d1 = f1;
    // float f2 = (f1-d1)*10;
    // int d2 = f2;
    // float f3 = (f2-d2)*10;
    // int d3 = f3;
    // char s[5];
    // s[0] = '0' + d1;
    // s[1] = '0' + d2;
    // s[2] = '.';
    // s[3] = '0' + d3;
    // s[4] = 0;
    // fprintf(stderr,"%s\n", s);
    union single_cast {
        float f;
        uint32_t i;
    };
    union single_cast floatcast;
    floatcast.i = *(sensordata);
    return std::make_pair(floatcast.f, true);
}

void read_sensor_hw2(volatile unsigned int *sensorvirt, float &value, bool &valid) {
    std::tie(value, valid) = read_sensor_hw(sensorvirt);
}

void get_ethernet_frame_direct(volatile unsigned int *networkIOPrecv, ap_uint<32>* buf, int* len, bool *b) {
#ifdef DEBUG
    printf("Getting... ");
#endif
    bool flag = (ap_uint<32>)networkIOPrecv[0x390] != 0;
    int length = 0;
    if(flag) {
        length = (ap_uint<32>)networkIOPrecv[0x394];
        for(int i = 0; i < (length+3)/4; i++) {
            buf[i] = (ap_uint<32>)networkIOPrecv[0x200+i]; // HACK!
        }
        networkIOPrecv[0x390] = 0;
    }
#ifdef DEBUG
    printf("Got %d.\n", *len);
#endif
    // if(*len < 0) {
    //     perror("Error receiving");
    // }
    *len = length;
    *b = (length > 0);
}

void put_ethernet_frame_direct(ap_uint<32>* buf, int* len, volatile unsigned int *networkIOPsend) {
#ifdef DEBUG
    printf("Putting %d... ", *len);
#endif
    int length = *len;
    if(length == 0) return;
    int i = 0;
    while((ap_uint<32>)networkIOPsend[0x190] != 0) {
        // Spin
        if(i++ > 1000000) {
            printf("timeout\n");
            break;
        }
        // wait
    }
    networkIOPsend[0x194] = length;
    for(int i = 0; i < (length+3)/4; i++) {
        networkIOPsend[i] = buf[i];
    }
    networkIOPsend[0x190] = 1;

#ifdef DEBUG
    printf("Done.\n");
#endif
}

void process_packet(bool b, ap_uint<48> macAddress, ap_uint<32> ipAddress, ap_uint<32> inBuf[4096], int *inLen,
                    int i, ap_uint<32> destIP, int destPort, ap_uint<16> topicID, int qos, float message, bool validMessage,
                    int count, int size, ap_uint<32> outBuf[4096], int *outLen, bool reset, bool _verbose, STATS &_stats) {
#pragma HLS inline
    static ArpCacheT arpcache;
    static MessageBuffer<std::pair<ap_uint<16>, float>, 128 > buffer;

    verbose=_verbose;
    if(reset) {
        stats.packets_received = 0;
        stats.packets_sent = 0;
        stats.arps_received = 0;
        stats.arps_sent = 0;
        stats.publishes_sent = 0;
        stats.dups_sent = 0;
        stats.acks_received = 0;
        stats.events_completed = 0;

        arpcache.clear();
        buffer.clear();
    }
    *outLen = 0;
    if(b) {
        stats.packets_received++;
        handle_ethernet_frame(macAddress, ipAddress, inBuf, *inLen, arpcache, buffer);
        return;
    }

    int tLen;

    int messageID;
    bool dup = false;
    if(validMessage) {
#pragma HLS inline all recursive
        if(qos > 0) {
            messageID = buffer.put(std::make_pair(topicID, message)); // Store the message so we can resend it.
#ifndef __SYNTHESIS__
            if(verbose) {
                std::cout << "putting ID=" << messageID << " size = " << buffer.size() << "\n";
            }
#endif
            if(messageID < 0) validMessage = false;
        } else {
            messageID = 0;
        }
        if(validMessage) {
            stats.publishes_sent++;
            if(qos == 0) {
                // no acknowledge
                stats.events_completed++;
            }
        }
    }
    if(!validMessage) {
#pragma HLS inline all recursive
        // If we don't have a valid message, or we were unable to put it into
        // the message buffer, then grab a message from the buffer and resend it.
        std::tie(topicID, message) = buffer.get(messageID);
        if(messageID >= 0) {
            validMessage = true;
#ifndef __SYNTHESIS__
            if(verbose) {
                std::cout << "resending ID=" << messageID << "\n";
            }
#endif
            dup = true;
        }
    }
    float f1 = message*0.1f;
    int d1 = f1;
    float f2 = (f1-d1)*10;
    int d2 = f2;
    float f3 = (f2-d2)*10;
    int d3 = f3;
    char s[4];
    s[0] = '0' + d1;
    s[1] = '0' + d2;
    s[2] = '.';
    s[3] = '0' + d3;

    if(validMessage) {
        test_source(i, macAddress, ipAddress, destIP, destPort,
                    s, 4, messageID, topicID,
                    qos, dup, size,
                    outBuf, tLen, arpcache);
        *outLen = tLen;
    }
    _stats = stats;
}

void read_and_process_packet(bool b, ap_uint<48> macAddress, ap_uint<32> ipAddress,
                             int i, ap_uint<32> destIP, int destPort, ap_uint<16> topicID, int qos, float message, bool validMessage, volatile unsigned int *networkIOP,
                             int count, int size, bool reset, bool _verbose, int &events_completed, int &publishes_sent, int &packets_received, int &packets_sent) {
    STATS _stats;
    //#pragma HLS dataflow
        // #pragma HLS interface m_axi offset=slave port=networkIOP bundle=networkIOP
// #pragma HLS interface s_axilite port=b
// #pragma HLS interface s_axilite port=macAddress
// #pragma HLS interface s_axilite port=ipAddress
// #pragma HLS interface s_axilite port=i`
// #pragma HLS interface s_axilite port=destIP
// #pragma HLS interface s_axilite port=destPort
// #pragma HLS interface s_axilite port=topicID
// #pragma HLS interface s_axilite port=qos
// #pragma HLS interface s_axilite port=message
// #pragma HLS interface s_axilite port=validMessage
// #pragma HLS interface s_axilite port=count
// #pragma HLS interface s_axilite port=size
// #pragma HLS interface s_axilite port=reset
// #pragma HLS interface s_axilite port=_verbose
// #pragma HLS interface s_axilite port=_stats

    ap_uint<32> inBuf[4096];
    int inLen;
    ap_uint<32> outBuf[4096];
    int outLen = 0;

    get_ethernet_frame_direct(networkIOP, inBuf, &inLen, &b);

    if(b) {
        //printf("Got packet %d.\n", i);
        //hexdump_ethernet_frame<16>(inBuf, 64);
    } else {
        // for(int i = 0; i < 16; i++) {
        //     inBuf[i] = 0;
        // }
    }

        //printf("processing..\n");
        if(size > 0) {
            // printf("Input Before len = %d.\n", inLen);
            // hexdump_ethernet_frame<16>(inBuf, 64);
            // printf("Output Before len = %d.\n", outLen);
            // hexdump_ethernet_frame<16>(outBuf, 64);
            // Send a new packet if there is nothing waiting to be acknowledged.
            //bool valid = _stats.publishes_sent < count; // (buffer.size() <= 200);
            process_packet(b, macAddress, ipAddress, inBuf, &inLen, i, destIP, destPort, topicID, qos, message, validMessage, count, size, outBuf, &outLen, reset, verbose, _stats);
            // printf("Input After len = %d.\n", inLen);
            // hexdump_ethernet_frame<16>(inBuf, 64);
            // printf("Output After len = %d.\n", outLen);
            // hexdump_ethernet_frame<16>(outBuf, 64);
        }
        //        if(count < 10000) break;

        if(outLen > 512) {
            //   printf("Bogus result length %d\n", outLen);
        } else {
            if(outLen > 0) {
                //  printf("Sending packet %d with %d bytes.\n", i, outLen);
                //hexdump_ethernet_frame<16>(outBuf, outLen);
                // sleep(2);
                put_ethernet_frame_direct(outBuf, &outLen, networkIOP);
            }
        }
        events_completed = _stats.events_completed;
        publishes_sent = _stats.publishes_sent;
        packets_received = _stats.packets_received;
        packets_sent = _stats.packets_sent;
}
