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
#define TEST test_writer1
#endif

const int OW = 56;
// class test_writer1 {
// public:
//     typedef ap_uint<OW> TYPE;
//     static void test(stream<axiWord> &dataIn, stream<ap_uint<OW> > &dataOut, int OBEATS) {
//         auto writer = make_writer(dataIn);
//         //        StreamReader<axiWord> r(dataIn);
//         //LittleEndianByteReader<StreamReader<axiWord> > writer(r);
//         for(int j = 0; j < OBEATS; j++) {
//             ap_uint<OW> x = writer.get<ap_uint<OW> >();
//             dataOut.write(x);
//         }
//     }
// };

class test_writer2 {
public:
    typedef ethernet::header TYPE;
    static void test(stream<ethernet::header> &dataIn, stream<axiWord> &dataOut, int OBEATS) {
        auto writer = make_writer(dataOut);
        for(int j = 0; j < OBEATS; j++) {
            ethernet::header x = dataIn.read();
            writer.put<ethernet::header>(x);
        }
    }
};
int main() {
#pragma HLS inline region off
	TEST::TYPE inData;
    axiWord outData;
	stream<TEST::TYPE> 		inFIFO("inFIFO");
	stream<axiWord > 		outFIFO("outFIFO");
	int 					errCount 					= 0;

    const int S = TEST::TYPE::LENGTH;
    for(int i = 40; i < 80; i++) {
        int count = 0;
        int count2 = 0;
        std::cout << "Testing Packet Length = " << std::dec << i << "\n";
        int BEATS = (i+S-1)/S; // Round up
        for(int j = 0; j < BEATS; j++) {
            for(int s = 0; s < S; s++) {
                inData.set<1>(s, count);
                count ++;
            }
            inFIFO.write(inData);
        }

        int OS = axiWord::WIDTH/8;
        int OBEATS = i/OS; // Round down
        // Run the test.
        TEST::test(inFIFO, outFIFO, BEATS);

        // Dump any remaining input data.
        while (!(inFIFO.empty())) {
            inFIFO.read();
        }
        //        assert(inFIFO.empty());
        while (!(outFIFO.empty())) {
            outData = outFIFO.read();
            // for(int s = 0; s < S; s++) {
            //    if(!outData.keep[s]) outData.data(8*s+7,8*s) = 0;
            // }
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
            for(int s = 0; s < OS; s++) {
                //  std::cout << outData(8*s+7,8*s) << " ";
                if(outData.data(s*8+7, s*8) != count2) {//outData(8*s+7,8*s) != count2) {
                    //                    std::cout << "(" << outData.get<1>(s) << "!=" << count2 << ")\n";
                    fail = true;
                    errCount ++;
                }
                if(true) {//outData.keep[s]) {
                    count2++;
                }
            }
            if (fail) {// || outData.last != outFIFO.empty()) {
                cout << "X";
            } else {
                cout << ".";
            }
            //std::cout << "\n";
        }
        // if(count2 != i) {
        //     cout << " Length Mismatch: " << std::dec << count2 << "!\n";
        //     errCount ++;
        // } else {
        //     cout << " done. \n";
        // }
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
