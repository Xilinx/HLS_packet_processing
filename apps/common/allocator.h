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

#pragma once

#include "hls_stream.h"
//#include "hls_math.h"
//#include "ip.hpp"

template <int N>
class Allocator;

template <int SIZE>
std::ostream& operator<<(std::ostream& os, const Allocator<SIZE>& a);

template <int N>
class Allocator {
    typedef ap_uint<BitWidth<N>::Value> indexT;
    indexT freehead;
    indexT freelist[N];
    indexT allocatedstack[N];
    indexT allocated_count;
    indexT findindex;
    bool allocated[N];

public:
    Allocator () {
#pragma HLS reset variable=freelist
#pragma HLS reset variable=allocated
#pragma HLS reset variable=allocated_count
        // Initialized the free list to contain everything but the last node.
        for(int i = 0; i < N-1; i++) {
#pragma HLS pipeline II=1
            freelist[i] = i+1;
        }
        freelist[N-1] = 0;
        freehead = 0;
        allocated_count = 0;
        findindex = 0;
    }
    void clear() {
        // Initialized the free list to contain everything but the last node.
        for(int i = 0; i < N-1; i++) {
#pragma HLS pipeline II=1
            freelist[i] = i+1;
        }
        freelist[N-1] = 0;
        freehead = 0;
        allocated_count = 0;
        findindex = 0;
    }
    bool is_allocated(int node) {
        return allocated[node];
    }
    int allocate() {
        indexT freenode = freelist[freehead];
        assert(!allocated[freenode]);
        if(freenode == freehead) {
            // fail, don't allocate anything.
            return -1;
        }
        freelist[freehead] = freelist[freenode];
        allocated[freenode] = true;
        // transfer it to the allocated list.
        allocatedstack[allocated_count] = freenode;
        freelist[freenode] = allocated_count; // Remember where the node is in the allocated stack.
        allocated_count++;
        return freenode;
    }
    void deallocate(int freenode) {
        assert(allocated[freenode]);
        assert(freenode >= 0);
        assert(freenode < N);
        assert(allocated_count > 0);
        assert(allocated_count <= N);
        // remove from the allocated stack.
        indexT stackloc = freelist[freenode];
        allocated_count--;
        indexT movednode = allocatedstack[stackloc] = allocatedstack[allocated_count];
        freelist[movednode] = stackloc;
        // Add back to the free list.
        freelist[freenode] = freelist[freehead];
        freelist[freehead] = freenode;
        allocated[freenode] = false;
    }
    int find_allocated() {
        findindex++;
        if(findindex >= allocated_count) {
            findindex = 0;
        }
        if(allocated_count > 0) {
            return allocatedstack[findindex];
        } else {
            return -1;
        }
    }
    int size() {
        return allocated_count;
    }
    friend std::ostream& operator<< <N>(std::ostream& os, const Allocator<N>& cam);
};
template <int SIZE>
std::ostream& operator<<(std::ostream& os, const Allocator<SIZE>& a) {
    int freenode = a.freelist[a.freehead];
    os << "Free:";
    os << " " << freenode;
    while(freenode != a.freehead) {
        freenode = a.freelist[freenode];
        os << " " << freenode;
    }
    os << "\nAllocated:";
    for(int i = 0; i < a.allocated_count; i++) {
        os << " " << a.allocatedstack[i];
    }
    os << "\n";
    return os;
}


// const static int memorySize = 256;
// struct PacketInMemory {
//     ap_uint<BitWidth<memorySize>::Value> index;
//     ap_uint<BitWidth<MTU>::Value> length;
// };

// /** @ingroup ip_handler
//  *  @param[in]		dataIn, incoming data stream
//  */
// template <typename PACKET>
// void packet2memory(PACKET p,
//                    hls::stream<PacketInMemory> &stream,
//                    ap_uint<axiWord::WIDTH> *data) {
//     static Allocator<256> allocator;
//     int index = allocator.allocate();

//     const int S = axiWord::WIDTH/8;
//     const int PacketOffset = 2048/S;
//     if(index != -1) {
//         for(int i = 0; i < (p.data_length()+S-1)/S; i++) {
//             ap_uint<axiWord::WIDTH> v;
//             ap_uint<S> k;
//             p.pop(v,k);
//             data[PacketOffset*index+i] = v;
//         }
//         PacketInMemory pim;
//         pim.index = index;
//         pim.length = p.data_length();
//         stream.write(pim);
//     } else {
//         // Drop
//     }
// }

// template <typename PACKET>
// void memory2packet(hls::stream<PacketInMemory> &stream,
//                    ap_uint<axiWord::WIDTH> *data,
//                    PACKET p) {
//     const int S = axiWord::WIDTH/8;
//     const int PacketOffset = 2048/S;
//     PacketInMemory pim;
//     pim = stream.read();
//     ap_uint<S> alignflag = generatekeep<S>(pim.length%S);
//     const int LBEATS = (pim.length + S - 1) / S;
//     for(int i = 0; i < LBEATS; i++) {
//         ap_uint<axiWord::WIDTH> v;
//         ap_uint<S> k;
//         v = data[PacketOffset*pim.index+i];
//         k = (i == LBEATS-1) ? alignflag : ap_uint<S>(-1);
//         p.push(v,k);
//     }
// }

template<typename T, int N>
class MessageBuffer {
public:
    Allocator<N> allocator;
    T data[N];

    MessageBuffer() { }
    bool is_allocated(int id) {
        return allocator.is_allocated(id);
    }
    // Put a new value t into the buffer.  If this succeeds, then return an id associated with the value in the buffer.
    // Otherwise return -1.
    int put(T t) {
        int id = allocator.allocate();
        if(id >= 0) data[id] = t;
        return id;
    }
    // Get a random value from the buffer along with it's id.  If no id is not currently allocated, then the return value is undefined and
    // id is set to -1.
    T get(int &id) {
        id = allocator.find_allocated();
        T t;
        if(id >= 0) {
            t = data[id];
        }
        return t;
    }
    void clear() {
        allocator.clear();
    }
    void clear(int id) {
        allocator.deallocate(id);
    }
    int size() {
        return allocator.size();
    }
};
