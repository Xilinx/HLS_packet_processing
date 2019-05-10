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

#include "cam.h"
#include <map>
#include <vector>
#include <iostream>
#include "hls_stream.h"
const static int N = 32768;
const static int keysize=32;
const static int valuesize=48;

void top3(hls::algorithmic_cam<N, 4, ap_uint<keysize>, ap_uint<valuesize> > mycam,
		hls::stream<ap_uint<keysize> > read_key_in,
         hls::stream<ap_uint<valuesize> > read_value_out) {

 get_loop: for(int i = 0; i < 16; i++) {
#pragma HLS pipeline II=1
#pragma HLS inline all recursive
     /*   if(!update_key_in.empty()) {
            ap_uint<keysize> kin = update_key_in.read();
            ap_uint<valuesize> vin = update_value_in.read();
            mycam.insert(kin, vin);
        }*/
        if(!read_key_in.empty()) {
            ap_uint<keysize> k = read_key_in.read();
            ap_uint<valuesize> v;
            bool b = mycam.get(k,v);
            if(b) {
                read_value_out.write(v);
            }
        }
    }
    mycam.sweep();
}


struct rtlMacLookupRequest {
	ap_uint<1>			source;
	ap_uint<32>			key;
	rtlMacLookupRequest() {}
	rtlMacLookupRequest(ap_uint<32> searchKey)
				:key(searchKey), source(0) {}
};

struct rtlMacUpdateRequest {
	ap_uint<1>			source;
	ap_uint<1>			op;
	ap_uint<48>			value;
	ap_uint<32>			key;

	rtlMacUpdateRequest() {}
	rtlMacUpdateRequest(ap_uint<32> key, ap_uint<48> value, ap_uint<1> op)
			:key(key), value(value), op(op), source(0) {}
};

struct rtlMacLookupReply {

	ap_uint<1>			hit;
	ap_uint<48>			value;
	rtlMacLookupReply() {}
	rtlMacLookupReply(bool hit, ap_uint<48> returnValue)
			:hit(hit), value(returnValue) {}
};

struct rtlMacUpdateReply {
	ap_uint<1>			source;
	ap_uint<1>			op;
	ap_uint<48>			value;

	rtlMacUpdateReply() {}
	rtlMacUpdateReply(ap_uint<1> op)
			:op(op), source(0) {}
	rtlMacUpdateReply(ap_uint<8> id, ap_uint<1> op)
			:value(id), op(op), source(0) {}
};
void top(hls::stream<rtlMacLookupRequest>	&LookupReq,
         hls::stream<rtlMacLookupReply>		&LookupResp,
         hls::stream<rtlMacUpdateRequest>	&UpdateReq,
         hls::stream<rtlMacUpdateReply>		&UpdateResp) {
	#pragma	HLS INTERFACE axis port=LookupReq	 bundle=LookupReq
	#pragma	HLS INTERFACE axis port=LookupResp	 bundle=LookupResp
	#pragma	HLS INTERFACE axis port=UpdateReq	 bundle=UpdateReq
	#pragma	HLS INTERFACE axis port=UpdateResp	 bundle=UpdateResp
	#pragma HLS DATA_PACK variable=LookupReq
	#pragma HLS DATA_PACK variable=LookupResp
	#pragma HLS DATA_PACK variable=UpdateReq
	#pragma HLS DATA_PACK variable=UpdateResp

    static hls::algorithmic_cam<N, 4, ap_uint<keysize>, ap_uint<valuesize> > mycam;

 get_loop: for(int i = 0; i < 16; i++) {
#pragma HLS pipeline II=1
#pragma HLS inline all recursive

        if(!LookupReq.empty()) {
            rtlMacLookupRequest req = LookupReq.read();
            ap_uint<keysize> k = req.key;
            ap_uint<valuesize> v;
            bool b = mycam.get(k,v);
            rtlMacLookupReply resp(b,v);
            LookupResp.write(resp);
        }
        if(!UpdateReq.empty()) {
            rtlMacUpdateRequest req = UpdateReq.read();
            ap_uint<keysize> kin = req.key;
            ap_uint<valuesize> vin = req.value;
            if(!req.op) {
                mycam.insert(kin, vin);
            } else {
                // Is this remove or get?
                mycam.remove(kin);
            }
            rtlMacUpdateReply resp(req.op);
            UpdateResp.write(resp);
        }
        else
            mycam.sweep2();
//         if(i%4) {
// #pragma HLS occurrence cycle=4
//             mycam.sweep();
//         }
    }
 // sweep_loop: for(int i = 0; i < 16; i++) {
 //        #pragma HLS pipeline II=1
 //        mycam.sweep();
 //    }
}

void top2(ap_uint<keysize> keys[N/2], ap_uint<valuesize> values[N/2], ap_uint<keysize> keysin[64], ap_uint<valuesize> valuesin[64], ap_uint<keysize> keysout[64], ap_uint<valuesize> valuesout[64]) {
   static hls::algorithmic_cam<N, 4, ap_uint<keysize>, ap_uint<valuesize> > mycam;

 insert_loop: for(int i = 0; i < N/2; i++) {
        ap_uint<keysize> k = keys[i];
        ap_uint<valuesize> v = values[i];
        mycam.insert(k,v);
        mycam.sweep();
     }

 get_loop: for(int i = 0; i < 64; i++) {
        #pragma HLS pipeline II=1
        ap_uint<keysize> kin = keysin[i];
        ap_uint<valuesize> vin = valuesin[i];
        //        mycam.insert(kin, vin);
        ap_uint<keysize> k = keysout[i];
        ap_uint<valuesize> v;
        bool b = mycam.get(k,v);
        mycam.sweep();
        valuesout[i] = v;
    }
}

template <typename MapT, typename CamT>
void check_consistency(MapT mymap, CamT mycam) {
    std::cout << "Checking consistency...\n";
    for(typename MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<8> v;
        bool b = mycam.get(i->first,v);
        if(!b) std::cout << "Failed: " << i->first << "->" << i->second << " but was " << v << "\n";
        assert(b);
    }
}
int main(int argv, char * argc[]) {
    // const int N = 16;
    // hls::cam<N, ap_uint<32>, ap_uint<8> > mycam;
    const int N = 64;
    hls::algorithmic_cam<N, 4, ap_uint<32>, ap_uint<8> > mycam;
    typedef std::map<ap_uint<32>, ap_uint<8> > MapT;
    MapT mymap;

    for(int i = 0; i < N/2; i++) {
        ap_uint<32> k = rand();
        ap_uint<8> v = rand();
        bool b = mycam.insert(k,v);
        mycam.sweep();
        mymap[k] = v;
        std::cout << "inserting " << k << "->" << v << "\n";
        std::cout << mycam << "\n";
        assert(b);
    }
    std::cout << mycam << "\n";
    check_consistency(mymap, mycam);

    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<32> k = i->first;
        ap_uint<8> v = rand();
        bool b = mycam.insert(k,v);
        mycam.sweep();
        mymap[k] = v;
        std::cout << "overwriting " << k << "->" << v << "\n";
        assert(b);
    }
    std::cout << mycam << "\n";
    check_consistency(mymap, mycam);

    for(int k = 0; k < 20; k++) {
        std::vector<ap_uint<32> > keystoremove;
        for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
            ap_uint<32> k = i->first;
            ap_uint<8> v = i->second;
            ap_uint<1> c = rand();
            if(c != 0) {
                bool b = mycam.remove(k);
                keystoremove.push_back(k);
                std::cout << "random removing " << k << "->" << v << "\n";
                assert(b);
            }
        }
        for(std::vector<ap_uint<32> >::iterator i = keystoremove.begin(); i != keystoremove.end(); i++) {
            mymap.erase(*i);
        }
        std::cout << mycam << "\n";
        check_consistency(mymap, mycam);

        for(int i = 0; i < N/2; i++) {
            ap_uint<32> k = rand();
            ap_uint<8> v = rand();
            ap_uint<1> c = rand();
            if(c != 0) {
                bool b = mycam.insert(k,v);
                mycam.sweep();
                mymap[k] = v;
                std::cout << "random inserting " << k << "->" << v << "\n";
                if(mymap.size() <= N) {
                    assert(b);
                } else {
                    // We've filled up the cam, so the insert should have failed.
                    std::cout << "filled up, dumping " << k << "->" << v << "\n";
                    mymap.erase(k);
                    assert(!b);
                }
            }
        }
        std::cout << mycam << "\n";
        check_consistency(mymap, mycam);
    }

    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<32> k = i->first;
        ap_uint<8> v = rand();
        bool b = mycam.remove(k);
        std::cout << "removing " << k << "->" << v << "\n";
        assert(b);
    }
    std::cout << mycam << "\n"; // Should be empty
    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<8> v;
        bool b = mycam.get(i->first,v);
        assert(!b); // Should have been removed
    }

}
