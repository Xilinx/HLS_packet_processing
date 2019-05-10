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

#include "ip.hpp"

using namespace hls;
using namespace std;

#define XSTR(x) #x
#define STR(x) XSTR(x)

#ifndef TEST
#define TEST test_parse1
#endif

class test_parse1 {
public:
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet h;
        h.deserialize(dataIn);
        h.serialize(dataOut);
    }
};
class test_parse2 {
public:
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        header<Packet, 20> h(p);
        std::cout << h.data_length() << "\n";
        h.deserialize(dataIn);
        auto h2 = ethernet::parse_ethernet_hdr(h);
        h2.set<1>(0,0);
        h.serialize(dataOut);
    }
};
class test_parse3 {
public:
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        header<Packet, 20> h(p);
        h.deserialize(dataIn);
        auto h2 = ipv4::parse_ipv4_hdr(h);
        h2.set<1>(0,0);
        h.serialize(dataOut);
    }
};
class test_parse4 {
public:
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        header<Packet, 20> h(p);
        h.deserialize(dataIn);
        auto h2 = icmp::parse_icmp_hdr(h);
        h2.set<2>(0,0);
        h.serialize(dataOut);
    }
};
class test_parse5 {
public:
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        header<Packet, 20> ih(p);
        ethernet_hdr<header<Packet, 20> > h(ih);
        h.deserialize(dataIn);
        auto h2 = arp::parse_arp_hdr(ih);
        h2.set<2>(0,0);
        h.serialize(dataOut);
    }
};
int main() {
#pragma HLS inline region off
	axiWord inData;
	axiWord outData;
	stream<axiWord> 		inFIFO("inFIFO");
	stream<axiWord> 		outFIFO("outFIFO");
	int 					errCount 					= 0;

    const int S = axiWord::WIDTH/8;
    for(int i = 40; i < 80; i++) {
        int count = 0;
        int count2 = 0;
        std::cout << "Testing Packet Length = " << std::dec << i << "\n";
        int BEATS = (i+S-1)/S;
        for(int j = 0; j < BEATS; j++) {
            for(int s = 0; s < S; s++) {
                inData.data(s*8+7, s*8) = count;
                inData.keep[s] = (count < i);
                count ++;
            }
            inData.last = (j == (BEATS-1));
            inFIFO.write(inData);
        }

        // Run the test.
        TEST::test(inFIFO, outFIFO);

        assert(inFIFO.empty());
        while (!(outFIFO.empty())) {
            outFIFO.read(outData);
            for(int s = 0; s < S; s++) {
                if(!outData.keep[s]) outData.data(8*s+7,8*s) = 0;
            }
            /* outputFile << std::hex << std::noshowbase;
            outputFile << std::setfill('0');
            outputFile << std::setw(8) << ((uint32_t) outData.data(63, 32));
            outputFile << std::setw(8) << ((uint32_t) outData.data(31, 0));
            outputFile << " " << std::setw(2) << ((uint32_t) outData.keep) << " ";
            outputFile << std::setw(1) << ((uint32_t) outData.last) << std::endl;
            goldenFile >> std::hex >> dataTemp >> keepTemp >> lastTemp;
            */
            // Compare results
            bool fail = false;
            for(int s = 0; s < S; s++) {
                if(outData.keep[s] && outData.data(8*s+7,8*s) != count2) {
                    fail = true;
                    errCount ++;
                }
                if(outData.keep[s]) {
                    count2++;
                }
            }
            if (fail || outData.last != outFIFO.empty()) {
                cout << "X";
            } else {
                cout << ".";
            }
        }
        if(count2 != i) {
            cout << " Length Mismatch: " << std::dec << count2 << "!\n";
            errCount ++;
        } else {
            cout << " done. \n";
        }
    }
    if (errCount == 0) {
    	cout << STR(TEST) ":*** Test Passed ***" << endl;
    	return 0;
    } else {
    	cout << STR(TEST) ":!!! TEST FAILED -- " << errCount << " mismatches detected !!!";
    	cout << endl;
    	return -1;
    }
	//should return comparison btw out.dat and out.gold.dat
	return 0;
}
