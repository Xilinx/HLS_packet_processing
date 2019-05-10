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

#include "ap_int.h"
#include "cam.h"
#include "allocator.h"

extern "C"
void Top(int size, int count,
         long long macAddress,
         int ipAddress,
         int destIP,
         int destPort,
         unsigned short topicID,
         int qos,
         bool verbose,
         volatile ap_uint<32> *networkIOP,
         volatile char *sensor);

extern "C"
float read_sensor(volatile char *sensor);

#pragma SDS data zero_copy(sensorvirt[0:4096])
#pragma SDS data mem_attribute(sensorvirt:NON_CACHEABLE)
//#pragma SDS data sys_port(sensorvirt:processing_system7_0_axi_periph_S01_AXI)
//#pragma SDS data sys_port(sensorvirt:processing_system7_0_S_AXI_HP2)
#pragma SDS data sys_port(sensorvirt:AFI)
std::pair<float,bool> read_sensor_hw(volatile unsigned int *sensorvirt);

#pragma SDS data zero_copy(sensorvirt[0:4096])
#pragma SDS data mem_attribute(sensorvirt:NON_CACHEABLE)
#ifdef PLATFORM_pynqz1
#pragma SDS data sys_port(sensorvirt:processing_system7_0_axi_periph_S01_AXI)
#endif
//#pragma SDS data sys_port(sensorvirt:processing_system7_0_S_AXI_HP2)
void read_sensor_hw2(volatile unsigned int *sensorvirt, float &value, bool &valid);

// void handle_ethernet_frame(ap_uint<48> macAddress, ap_uint<32> ipAddress, ap_uint<32> *buf, int len,
//                            hls::algorithmic_cam<256, 4, MacLookupKeyT, MacLookupValueT> &cam,
//                            MessageBuffer<std::pair<ap_uint<16>, float>, 256 > &buffer);

/*void test_source(int i, ap_uint<48> macAddress, ap_uint<32> ipAddress, ap_uint<32> destIP, int destPort,
                 char *s, int sLen,
                 ap_uint<16> topicID, int size,
                 ap_uint<32> *buf, int &lenm,
                 hls::algorithmic_cam<256, 4, MacLookupKeyT, MacLookupValueT> &cam);
*/
typedef struct {
    int packets_received;
    int packets_sent;
    int arps_received;
    int arps_sent;
    int publishes_sent;
    int dups_sent;
    int acks_received;
    int events_completed;
} STATS;

#ifdef RAW
#pragma SDS data zero_copy(inBuf[0:4096])
#pragma SDS data zero_copy(outBuf[0:4096])
//#pragma SDS data zero_copy(sensor[0:4096])
//#pragma SDS data mem_attribute(inBuf:NON_CACHEABLE)
//#pragma SDS data mem_attribute(outBuf:NON_CACHEABLE)
//#pragma SDS data mem_attribute(sensor:NON_CACHEABLE)
#ifdef PLATFORM_pynqz1
//#pragma SDS data sys_port(sensor:processing_system7_0_S_AXI_HP2)
//#pragma SDS data sys_port(inBuf:processing_system7_0_S_AXI_HP0)
//#pragma SDS data sys_port(outBuf:processing_system7_0_S_AXI_HP0)
  //#pragma SDS data sys_port(sensor:processing_system7_0_axi_periph_S01_AXI)
#endif
#else
#pragma SDS data copy(b[0:1], inBuf[0:1024], inLen[0:1], outBuf[0:1024], outLen[0:1])
#pragma SDS data access_pattern(b: SEQUENTIAL, inBuf:SEQUENTIAL, inLen:SEQUENTIAL, outBuf:SEQUENTIAL, outLen:SEQUENTIAL)
#endif
extern void process_packet(bool b, ap_uint<48> macAddress, ap_uint<32> ipAddress, ap_uint<32> *inBuf, int *inLen,
                    int i, ap_uint<32> destIP, int destPort,
                    ap_uint<16> topicID, int qos,
                    float message, bool messageValid,
                    int count, int size, ap_uint<32> *outBuf, int *outLen, bool reset, bool verbose, STATS &stats);

//#pragma SDS data zero_copy(sensor[0:4096])
#pragma SDS data zero_copy(networkIOP[0:4096])
//#pragma SDS data mem_attribute(inBuf:NON_CACHEABLE)
//#pragma SDS data mem_attribute(outBuf:NON_CACHEABLE)
//#pragma SDS data mem_attribute(sensor:NON_CACHEABLE)
#pragma SDS data mem_attribute(networkIOP:NON_CACHEABLE)
#ifdef PLATFORM_pynqz1
//#pragma SDS data sys_port(sensor:processing_system7_0_S_AXI_HP2)
//#pragma SDS data sys_port(inBuf:processing_system7_0_S_AXI_HP0)
//#pragma SDS data sys_port(outBuf:processing_system7_0_S_AXI_HP0)
#pragma SDS data sys_port(networkIOP:processing_system7_0_axi_periph_S01_AXI)
#endif
void read_and_process_packet(bool b, ap_uint<48> macAddress, ap_uint<32> ipAddress,
                             int i, ap_uint<32> destIP, int destPort, ap_uint<16> topicID, int qos, float message, bool messageValid,
                             volatile unsigned int *networkIOP,
                             int count, int size, bool reset, bool _verbose, int &events_completed, int &publishes_sent, int &packets_received, int &packets_sent); //, STATS &_stats);

