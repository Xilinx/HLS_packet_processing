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

#include <vector>
#include <set>
#include <iostream>
#include "hls_stream.h"
#include "allocator.h"

int main(int argv, char * argc[]) {
    const int N = 8;
    typedef std::set<int> SetT;
    SetT myset;
    Allocator<N> allocator;
    for(int i = 0; i < N-1; i++) {
        int n = allocator.allocate();
        std::cout << "allocating " << n << "\n";
        std::cout << allocator;
        assert(n >= 0);
        assert(n < N);
        assert(myset.count(n) == 0);
        myset.insert(n);
    }
    assert(allocator.allocate() == -1);

    bool done = false;
    while(!done) {
        int n = allocator.find_allocated();
        std::cout << "found " << n << "\n";
        if(n < 0) {
            break;
        } else {
            allocator.deallocate(n);
            std::cout << "deallocating " << n << "\n";
            assert(myset.count(n) == 1);
            myset.erase(myset.find(n));
        }
    }
    // for(SetT::iterator i = myset.begin(); i != myset.end(); i++) {
    //     allocator.deallocate(*i);
    // }
    myset.clear();
    std::cout << allocator;

    for(int i = 0; i < N-1; i++) {
        int n = allocator.allocate();
        std::cout << "allocating " << n << "\n";
        std::cout << allocator;
        assert(n >= 0);
        assert(n < N);
        assert(myset.count(n) == 0);
        myset.insert(n);
    }
    assert(allocator.allocate() == -1);

    for(int k = 0; k < 20; k++) {
        std::vector<ap_uint<32> > keystoremove;
        for(SetT::iterator i = myset.begin(); i != myset.end(); i++) {
            ap_uint<32> n = *i;
            ap_uint<1> c = rand();
            if(c != 0) {
                std::cout << "random removing " << n << "\n";
                allocator.deallocate(n);
                std::cout << allocator;
                keystoremove.push_back(n);
                //       assert(b);
            }
        }
        for(std::vector<ap_uint<32> >::iterator i = keystoremove.begin(); i != keystoremove.end(); i++) {
            myset.erase(*i);
        }

        for(int i = 0; i < N-1; i++) {
            int n = allocator.allocate();
            std::cout << "allocating " << n << "\n";
            std::cout << allocator;
            if(n < 0) break;
            assert(n < N);
            assert(myset.count(n) == 0);
            myset.insert(n);
        }
    }

    for(SetT::iterator i = myset.begin(); i != myset.end(); i++) {
        int k = *i;
        std::cout << "final removing " << k << "\n";
        allocator.deallocate(k);
        std::cout << allocator;
     }
    myset.clear();
    // //    std::cout << mycam << "\n"; // Should be empty
    // for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
    //     ap_uint<valuesize> v;
    //     send_lookup(i->first);
    // }
    // for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
    //     top(LookupReq,
    //         LookupResp,
    //         UpdateReq,
    //         UpdateResp);
    // }
    // for(MapT::iterator i = mymap.begin(); i != mymap.end(); i++) {
    //     ap_uint<valuesize> v;
    //     bool b = receive_lookup(v);
    //     assert(!b); // Should have been removed
    // }
}
