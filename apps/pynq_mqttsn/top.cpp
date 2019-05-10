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
#include <tuple>

#ifdef __SDSCC__
#include "sds_lib.h"
#else
#define sds_alloc malloc
#define sds_free free
#endif

#include "app.h"
#include "eth_interface.h"
#include "ip.hpp"
#include "cam.h"

//#define DEBUG

// The input pointer here is a virtual address to the memory of the
// appropriate IOP
//extern "C"
float read_sensor(volatile char *sensorvirt) {
    float f;
    bool valid;
    int i = 0xF000;
    // {
    //     volatile unsigned int *v2 = (volatile unsigned int *)sensorvirt;
    //     unsigned int t = *(v2+i/4);
    //     volatile float *f2 = (volatile float *)sensorvirt;
    //     float t2 = f2[i/4];
    //     printf("sw: %x %x %f\n", i, t, t2);
    // }
    // volatile unsigned int *v3 = (volatile unsigned int *) sds_mmap((void *)0x42000000, 65536, (void *)sensorvirt);
    // std::tie(f, valid) = read_sensor_hw((volatile unsigned int *)sensorvirt);
    // printf("sw2:%f %d\n", f, valid);
    // std::tie(f, valid) = read_sensor_hw((volatile unsigned int *)v3);
    // printf("sw2:%f %d\n", f, valid);
    // read_sensor_hw2(v3, f, valid);
    // printf("hw: %x %x %f %d\n", v3, f, f, valid);
    read_sensor_hw2((volatile unsigned int *)sensorvirt, f, valid);
    for(int j =0; j < 1000; j++) {
        volatile float *f2 = (volatile float *)sensorvirt;
        f = f2[i/4];
    }
    //    printf("hw: %x %x %f %d\n", sensorvirt, f, f, valid);
    return f;
}

//extern "C"
void Top(int size, int count,
         long long macAddress,
         int ipAddress,
         int destIP,
         int destPort,
         unsigned short topicID,
         int qos,
         bool verbose,
         volatile ap_uint<32> *networkIOP,
         volatile char *sensor) {
    try {
    printf("macAddress = %Lx\n", macAddress);
    printf("ipAddress = %x\n", ipAddress);
    printf("destIP = %x\n", destIP);
    //    sleep(2);
    // printf("networkIOP = %x\n", networkIOP);
    // printf("sensorIOP = %x\n", sensor);

    // for(int i = 0; i < 1024; i++) {
    //     outBuf[i] = 0xDEADBEEF;
    // }
    // unsigned int a = (ap_int<32>)networkIOP[0x390];
    // unsigned int b = (ap_int<32>)networkIOP[0x394];
    // printf("packet = %d %d\n", a, b);

    int packetsequence = 0;

    int hang = 0;
    STATS stats = {};
    stats.events_completed = 0;
    stats.publishes_sent = 0;
    bool first = true;
    while(stats.events_completed < count && hang++ < 100000) {
        int i = stats.events_completed;
        bool b = false;
        unsigned dummyfifo[1];
        unsigned dummyfifo2[1];
        float message = 0.0f;
        bool valid = false;
        if(stats.publishes_sent < count) {
            // attempt to read the sensor and send a message.
            std::tie(message, valid) = read_sensor_hw((volatile unsigned int *)sensor);
        }
        int events_completed;
        int publishes_sent;
        int packets_received;
        int packets_sent;
        //if(valid) {std::cout << "v";}
        read_and_process_packet(b, macAddress, ipAddress, i, destIP, destPort, topicID, qos,
                                message, valid,
                                (volatile unsigned int *)networkIOP,
                                count, size, first, verbose, events_completed, publishes_sent, packets_received, packets_sent);

        stats.events_completed = events_completed;
        stats.publishes_sent = publishes_sent;
        stats.packets_received = packets_received;
        stats.packets_sent = packets_sent;
        first = false;
    }

    std::cout << "Sent: " << stats.packets_sent << "\n"
              << "Recv: " << stats.packets_received << "\n"
              << "ArpsSent: " << stats.arps_sent << "\n"
              << "ArpsRecv: " << stats.arps_received << "\n"
              << "PubsSent: " << stats.publishes_sent << "\n"
              << "DupsSent: " << stats.dups_sent << "\n"
              << "AcksRecv: " << stats.acks_received << "\n"
              << "Complete: " << stats.events_completed << "\n";
    } catch(std::exception &e) {
        std::cout << "Caught std::exception " << e.what() << "\n";
    } catch(...) {
        std::cout << "Caught Exception\n" ;
    }
}

