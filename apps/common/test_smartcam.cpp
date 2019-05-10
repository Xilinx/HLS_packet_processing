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
const static int N = 16384;
const static int keysize=128;
const static int valuesize=32;


/** @ingroup session_lookup_controller
 *
 */
enum lookupSource {RX, TX_APP};

/** @ingroup session_lookup_controller
 *
 */
enum lookupOp {INSERT, DELETE};

struct slupRouting
{
	bool			isUpdate;
	lookupSource 	source;
	slupRouting() {}
	slupRouting(bool isUpdate, lookupSource src)
			:isUpdate(isUpdate), source(src) {}
};

/** @ingroup session_lookup_controller
 *  This struct defines the internal storage format of the IP tuple instead of destiantion and source,
 *  my and their is used. When a tuple is sent or received from the tx/rx path it is mapped to the fourTuple struct.
 *  The < operator is necessary for the c++ dummy memory implementation which uses an std::map
 */
struct fourTupleInternal
{
	ap_uint<32>	myIp;
	ap_uint<32> theirIp;
	ap_uint<16>	myPort;
	ap_uint<16> theirPort;
	fourTupleInternal() {}
	fourTupleInternal(ap_uint<32> myIp, ap_uint<32> theirIp, ap_uint<16> myPort, ap_uint<16> theirPort)
	: myIp(myIp), theirIp(theirIp), myPort(myPort), theirPort(theirPort) {}

	bool operator<(const fourTupleInternal& other) const
	{
		if (myIp < other.myIp)
		{
			return true;
		}
		else if (myIp == other.myIp)
		{
			if (theirIp < other.theirIp)
			{
				return true;
			}
			else if(theirIp == other.theirIp)
			{
				if (myPort < other.myPort)
				{
					return true;
				}
				else if (myPort == other.myPort)
				{
					if (theirPort < other.theirPort)
					{
						return true;
					}
				}
			}
		}
		return false;
	}
};

/** @ingroup session_lookup_controller
 *
 */
struct rtlSessionLookupRequest
{
	fourTupleInternal	key;
	lookupSource		source;
	rtlSessionLookupRequest() {}
	rtlSessionLookupRequest(fourTupleInternal tuple, lookupSource src)
				:key(tuple), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct rtlSessionUpdateRequest
{
    fourTupleInternal	key;
	ap_uint<14>			value;
    lookupOp			op;
	lookupSource		source;

	/*ap_uint<14>			value;
	lookupOp			op;
	lookupSource		source;*/
	rtlSessionUpdateRequest() {}
	/*rtlSessionUpdateRequest(fourTupleInternal key, lookupSource src)
				:key(key), value(0), op(INSERT), source(src) {}*/
	rtlSessionUpdateRequest(fourTupleInternal key, ap_uint<14> value, lookupOp op, lookupSource src)
			:key(key), value(value), op(op), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct rtlSessionLookupReply
{
	bool				hit;
	ap_uint<14>			sessionID;
	lookupSource		source;
	rtlSessionLookupReply() {}
	rtlSessionLookupReply(bool hit, lookupSource src)
			:hit(hit), sessionID(0), source(src) {}
	rtlSessionLookupReply(bool hit, ap_uint<14> id, lookupSource src)
			:hit(hit), sessionID(id), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct rtlSessionUpdateReply
{
	// lookupSource		source;
	// lookupOp			op;
	ap_uint<14>			sessionID;
	lookupOp			op;
	lookupSource		source;
	rtlSessionUpdateReply() {}
	rtlSessionUpdateReply(lookupOp op, lookupSource src)
			:op(op), source(src) {}
	rtlSessionUpdateReply(ap_uint<14> id, lookupOp op, lookupSource src)
			:sessionID(id), op(op), source(src) {}
};

// template <typename T>
// T reg(T t) {
// #pragma HLS inline self off
// #pragma HLS interface variable=return register
//     return t;
// }

void test(int ir, int iw, int t[16], int in) {
    static int y;
    int z;
    int a0, a1, a2, a3, a4, a5, a6;
    a0 = a1 = a2 = a3 = a4 = a5 = a6 = 0;
    for(int i = 0; i < 16; i++) {
        #pragma HLS pipeline II=1
        a6 = a5;
        a5=a4;
        a4=a3;
        a3=a2;
        a2=a1;
        a1=a0;
        t[i] = y;
        a0 = reg(reg(reg(t[i])));
        y = a6;
    }
}


typedef hls::SmartCam<N, ap_uint<keysize>, ap_uint<valuesize> > CAM_T;

hls::stream<CAM_T::LookupRequest>	LookupReq;
hls::stream<CAM_T::LookupReply>		LookupResp;
hls::stream<CAM_T::UpdateRequest>	UpdateReq;
hls::stream<CAM_T::UpdateReply>		UpdateResp;

void send_insert(ap_uint<keysize> k, ap_uint<valuesize> v) {
    CAM_T::UpdateRequest req;
    req.key = k;
    req.value = v;
    req.op = 0;
    UpdateReq.write(req);
}
void send_remove(ap_uint<keysize> k) {
    CAM_T::UpdateRequest req;
    req.key = k;
    req.op = 1;
    UpdateReq.write(req);
}
void send_lookup(ap_uint<keysize> k) {
    CAM_T::LookupRequest req;
    req.key = k;
    LookupReq.write(req);
}
void receive_update() {
     UpdateResp.read();
}
bool receive_lookup(ap_uint<valuesize> &value) {
    CAM_T::LookupReply resp;
    resp = LookupResp.read();
    value = resp.value;
    return resp.hit;
}

void top(hls::stream<CAM_T::LookupRequest>	&LookupReq,
                hls::stream<CAM_T::LookupReply>		&LookupResp,
                hls::stream<CAM_T::UpdateRequest>	&UpdateReq,
                hls::stream<CAM_T::UpdateReply>		&UpdateResp) {
    CAM_T::top(LookupReq, LookupResp, UpdateReq, UpdateResp);
}

template <typename MapT>
void check_consistency(MapT mymap) {
    std::cout << "Checking consistency...\n";
    for(typename MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        send_lookup(i->first);
    }
    for(typename MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        top(LookupReq,
            LookupResp,
            UpdateReq,
            UpdateResp);
    }
    for(typename MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<valuesize> v;
        bool b = receive_lookup(v);
        std::cout << "Testing " << i->first << "\n";
        if(!b || i->second != v) {
            std::cout << "Failed: " << i->first << "->" << i->second << " but was " << v << "\n";
            assert(false);
        }
    }
}

int main(int argv, char * argc[]) {
     const int N = 16;
     //     hls::cam<N, ap_uint<32>, ap_uint<8> > mycam;
     //const int N = 64;
     //hls::algorithmic_cam<N, 4, ap_uint<32>, ap_uint<8> > mycam;
    typedef std::map<ap_uint<32>, ap_uint<8> > MapT;
    MapT mymap;

    for(int i = 0; i < N/2; i++) {
        ap_uint<32> k = rand();
        ap_uint<8> v = rand();
        std::cout << "inserting " << k << "->" << v << "\n";
        send_insert(k,v);
        for(int i = 0; i < 16; i++)
            top(LookupReq,
                LookupResp,
                UpdateReq,
                UpdateResp);
        receive_update();
        mymap[k] = v;
        //std::cout << mycam << "\n";
        //assert(b);
    }
    //    std::cout << mycam << "\n";
    check_consistency(mymap);

    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<32> k = i->first;
        ap_uint<8> v = rand();
        std::cout << "overwriting " << k << "->" << v << "\n";
        send_insert(k,v);
        for(int i = 0; i < 16; i++)
            top(LookupReq, 
                LookupResp,
                UpdateReq,
                UpdateResp);
        receive_update();
        mymap[k] = v;
        //     assert(b);
    }
    //    std::cout << mycam << "\n";
    check_consistency(mymap);

    for(int k = 0; k < 20; k++) {
        std::vector<ap_uint<32> > keystoremove;
        for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
            ap_uint<32> k = i->first;
            ap_uint<8> v = i->second;
            ap_uint<1> c = rand();
            if(c != 0) {
                std::cout << "random removing " << k << "->" << v << "\n";
                send_remove(k);
                for(int i = 0; i < 16; i++)
                    top(LookupReq,
                        LookupResp,
                        UpdateReq,
                        UpdateResp);
                receive_update();
                keystoremove.push_back(k);
                //       assert(b);
            }
        }
        for(std::vector<ap_uint<32> >::iterator i = keystoremove.begin(); i != keystoremove.end(); i++) {
            mymap.erase(*i);
        }
        //        std::cout << mycam << "\n";
        check_consistency(mymap);

        for(int i = 0; i < N/2; i++) {
            ap_uint<32> k = rand();
            ap_uint<8> v = rand();
            ap_uint<1> c = rand();
            if(c != 0) {
                std::cout << "random inserting " << k << "->" << v << "\n";
                send_insert(k,v);
                for(int i = 0; i < 16; i++)
                    top(LookupReq, 
                        LookupResp,
                        UpdateReq,
                        UpdateResp);
                receive_update();
                mymap[k] = v;
                if(mymap.size() <= N) {
                    //     assert(b);
                } else {
                    // We've filled up the cam, so the insert should have failed.
                    std::cout << "filled up, dumping " << k << "->" << v << "\n";
                    mymap.erase(k);
                    // assert(!b);
                }
            }
        }
        //        std::cout << mycam << "\n";
        check_consistency(mymap);
    }

    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<32> k = i->first;
        ap_uint<8> v = rand();
        std::cout << "final removing " << k << "->" << v << "\n";
        send_remove(k);
        for(int i = 0; i < 16; i++)
            top(LookupReq,
                LookupResp,
                UpdateReq,
                UpdateResp);
        receive_update();
        //assert(b);
    }
    //    std::cout << mycam << "\n"; // Should be empty
    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<valuesize> v;
        send_lookup(i->first);
    }
    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        top(LookupReq,
            LookupResp,
            UpdateReq,
            UpdateResp);
    }
    for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
        ap_uint<valuesize> v;
        bool b = receive_lookup(v);
        assert(!b); // Should have been removed
    }
}
