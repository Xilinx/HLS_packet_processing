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
#define TEST test_prepend_noset1
#endif

class test_prepend_noset1 {
public:
    static const int LENGTH = 14;
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        ethernet_hdr<Packet> h(p);
        p.deserialize(dataIn);
        // for(int i = 0; i < LENGTH; i++) {
        //     h.set<1>(i, i-(LENGTH));
        // }
        h.serialize(dataOut);
    }
};
class test_prepend_noset2 {
public:
    static const int LENGTH = 20;
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        ipv4_hdr<Packet> h(p);
        p.deserialize(dataIn);
        // for(int i = 0; i < LENGTH; i++) {
        //     h.set<1>(i, i-(LENGTH));
        // }
        h.serialize(dataOut);
    }
};
class test_prepend_noset3 {
public:
    static const int LENGTH = 8;
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        icmp_hdr<Packet> h(p);
        p.deserialize(dataIn);
        // for(int i = 0; i < LENGTH; i++) {
        //     h.set<1>(i, i-(LENGTH));
        // }
        h.serialize(dataOut);
    }
};
class test_prepend_noset4 {
public:
    static const int LENGTH = 28;
    static void test(stream<axiWord> &dataIn, stream<axiWord> &dataOut) {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        icmp_hdr<ipv4_hdr<Packet> > ic(ih);
        ipv4_hdr<icmp_hdr<ipv4_hdr<Packet> > > h(ic);
        ih.deserialize(dataIn);
        // for(int i = 0; i < LENGTH; i++) {
        //     h.set<1>(i, i-(LENGTH));
        // }
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
        int count2 = 0-(TEST::LENGTH);
        int value = 0;
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
                ap_uint<8> val = (count2 < 0)?0:count2;
                if(outData.keep[s] && outData.data(8*s+7,8*s) != ap_uint<8>(value)) {
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
