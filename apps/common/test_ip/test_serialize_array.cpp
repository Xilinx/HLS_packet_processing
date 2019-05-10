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
#define TEST test_serialize_array1
#endif

#define WIDTH 128
typedef ap_axiu<WIDTH, 1, 1,1> DataBeat;

// Read data from the packet stream at the end of this packet.
// Store the computed checksum.
template<typename T, int W>
void parse(T &packet, ap_uint<W> *array, int len) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
    // Compute the checksum of the data as we read it.
    IPChecksum<W> csum;
    // The number of bytes in each data beat.
    const int S = W/8;
    // The number of bytes read from the input
    ap_uint<BitWidth<MTU+S>::Value> bytesread = 0;

    ap_uint<W> data;
    ap_uint<S> keep;
    bool last = false;
    packet.clear();
    bool notdone = false;
    int bound = len/S;
 PARSE: for(int count = 0; count < len; count++) {
    //while (!last || notdone) {
#pragma HLS pipeline II = 1
        //#pragma HLS inline all recursive
        data = array[count];
        keep = -1;
        // if(!last) {
        //     last = (bytesread + S) >= len;
        //     ap_uint<BitWidth<S>::Value> end(len%S);
        //     keep = last ? generatekeep<S>(end) : ap_uint<S>(-1);
        //     // std::cout << std::hex << "D:" << data << " " << keep << "\n";
        //     if (bytesread > MTU) {
        //         std::cout << "MTU reached.\n";
        //         //  break;
        //     }
        //     assert(keep == ap_uint<S>(-1) || last);
        //     csum.add_data_network_byte_order(data);
        // } else {
        //     keep = 0;
        // }
        bytesread += keptbytes(keep);
        packet.push(data, keep);
        //   notdone = packet.haspushdatatoflush();
    }
    packet.checksum = csum.get();
#ifdef DEBUG
    std::cout << "deserialized " << std::dec << bytesread << " bytes.\n";
#endif
    //std::cout << "deserialized " << std::dec << bytesread << " bytes on " << stream.get_name() << ".\n";
    packet.verify_consistency();
    //std::cout << "p.length = " << std::dec << p.data_length() << "\n";
}

//using namespace mqttsn;

class test_serialize_array1 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet h;
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
        parse(h, dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array2 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        ethernet_hdr<Packet> h(p);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array3 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        ipv4_hdr<Packet> h(p);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array4 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        icmp_hdr<Packet> h(p);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
        h.deserialize(dataIn, lengthIn);
        h.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array5 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        icmp_hdr<Packet> h(p);
        ipv4_hdr<icmp_hdr<Packet> > ih(h);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array6 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        header<Packet,3> h(p);
        header<header<Packet,3>,2 > ih(h);
        //        mqttsn_publish_hdr<Packet> h(p);
        //mqttsn_hdr<mqttsn_publish_hdr<Packet> > ih(h);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
    }
};
class test_serialize_array7 {
public:
    static void test(ap_uint<WIDTH> *dataIn, int lengthIn,
                     ap_uint<WIDTH> *dataOut, int &lengthOut) {
        Packet p;
        header<Packet,3> h(p);
        header<header<Packet,3>,2 > mh(h);
        ipv4_hdr< header<header<Packet,3>,2 > > ih(mh);
        //mqttsn_publish_hdr<Packet> h(p);
        //mqttsn_hdr<mqttsn_publish_hdr<Packet> > mh(h);
        //ipv4_hdr<mqttsn_hdr<mqttsn_publish_hdr<Packet> > > ih(mh);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
        ih.deserialize(dataIn, lengthIn);
        ih.serialize(dataOut, lengthOut);
    }
};

void test_wrapper(ap_uint<WIDTH> *dataIn, int lengthIn,
                  ap_uint<WIDTH> *dataOut, int &lengthOut) {
#pragma HLS interface port=dataIn m_axi depth=2048 latency=20
#pragma HLS interface port=dataOut m_axi depth=2048 latency=20
    TEST::test(dataIn, lengthIn, dataOut, lengthOut);
}

int main() {
#pragma HLS inline region off
	DataBeat inData;
	DataBeat outData;
    ap_uint<WIDTH> inArray[4096];
    int inLength;
    ap_uint<WIDTH> outArray[4096];
    int outLength;
    int 					errCount 					= 0;

    const int S = WIDTH/8;
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
            inArray[j] = inData.data;
        }

        inLength = i;
        // Run the test.
        TEST::test(inArray, inLength,
                   outArray, outLength);

        if(outLength != inLength) {
            cout << " Length Mismatch: " << std::dec << inLength << "!=" << outLength << "!\n";
            errCount ++;
        }
        for(int j = 0; j < outLength;) {
            // Compare results
            bool fail = false;
            for(int s = 0; s < S; s++) {
                if(j < outLength && (outArray[j/S])(8*s+7,8*s) != j) {
                    cout << (outArray[j/S])(8*s+7,8*s) << " " << j << "\n";
                    fail = true;
                    errCount ++;
                }
                j++;
            }
            if (fail) {
                cout << "X";
            } else {
                cout << ".";
            }
            // for(int i =0; i < 10; i++) {
            //     cout << outArray[i] << "\n";
            //     cout << inArray[i] << "\n";
            // }
        }
        cout << " done. \n";
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
