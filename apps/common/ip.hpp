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

/** There are two primary concepts in this code: Headers and Payloads
 *  A Header is a fixed-length portion at the start of the packet.
 *  The header is optimized for multiple reads and writes per clock and
 *  constant-index access.  Variable-index access is possible but has a
 *  resource cost which is roughly O(N), where N is the size of the header.
 *  A Payload represents the non-header portion of the packet.  Payloads
 *  support constant and variable-index access with low resource cost, but
 *  can only support one access per clock cycle.
 *  Several operations are common.
 *  deserialization: Converting a stream of data beats into a Header and Payload.
 *  serialization: Converting a Header and Payload into a stream of data beats.
 *  Serialization and Deserialization imply a basic Width, which affects the overall
 *  throughput of the system.
 *  A particular problem is serialization and deserialization when interfaces are
 *  different widths (in particular, when one width is not divisible by the other width)
 *  Header addition: Adding a new Header
 *  Header removal: removing a header
 *  A particular problem is efficiently supporting adding and removing headers and
 *  serialization and deserialization where the length of the header is not divisible
 *  by the width of the (de)serialization.
 *  Another particular problem is when the length of a header is less than the width of
 *  deserialization.
 *  Interpreting a header: A header can always be cast to a header of a different
 *  type with smaller size.
 *  Headers are composed of fields.
 */
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"

#include "hls/utils/x_hls_utils.h"
#include "assert.h"
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <math.h>
#include <hls_stream.h>
#include "ap_int.h"
#include <stdint.h>
#include <cstdlib>
#include "ap_axi_sdata.h"
#include <boost/mpl/vector.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/assert.hpp>
//#include <boost/mpl/vector_c.hpp>
#include <boost/mpl/iterator_range.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/string.hpp>
#include <boost/mpl/not.hpp>
#include <boost/type_traits/is_same.hpp>
#ifndef __SYNTHESIS__
#include <iostream>
#endif
using namespace hls;

#define WORKAROUND

#define MTU 9000


class MACAddressT: public ap_uint<48> {
    //    using ap_uint<48>::ap_uint;
public:
    MACAddressT(): ap_uint<48>(0) {}
    template<typename T> MACAddressT(T x): ap_uint<48>(x) {}
};

static std::ostream& operator<<(std::ostream& os, const MACAddressT& x) {
#ifndef __SYNTHESIS__
    std::ios oldState(nullptr);
    oldState.copyfmt(os);

    os << std::hex << std::setw(2) << std::setfill('0') <<
        (int)x(47,40) << ":" <<
        (int)x(39,32) << ":" <<
        (int)x(31,24) << ":" <<
        (int)x(23,16) << ":" <<
        (int)x(15,8) << ":" <<
        (int)x(7,0);
    os.copyfmt(oldState);
#endif
    return os;
}

class IPAddressT: public ap_uint<32> {
    //    using ap_uint<32>::ap_uint;
public:
    IPAddressT(): ap_uint(0) {}
    template<typename T> IPAddressT(T x): ap_uint(x) {}
};

static std::ostream& operator<<(std::ostream& os, const IPAddressT& x) {
#ifndef __SYNTHESIS__
    std::ios oldState(nullptr);
    oldState.copyfmt(os);
    os << std::dec <<
        x(31,24) << "." <<
        x(23,16) << "." <<
        x(15,8) << "." <<
        x(7,0);
    os.copyfmt(oldState);
#endif
    return os;
}


static ap_uint<48> byteSwap48(ap_uint<48> inputVector) {
#pragma HLS inline
	return (inputVector.range(7,0), inputVector(15, 8), inputVector(23, 16), inputVector(31, 24), inputVector(39, 32), inputVector(47, 40));
}
static ap_uint<32> byteSwap32(ap_uint<32> inputVector) {
#pragma HLS inline
	return (inputVector.range(7,0), inputVector(15, 8), inputVector(23, 16), inputVector(31, 24));
}
static ap_uint<16> byteSwap16(ap_uint<16> inputVector) {
#pragma HLS inline
	return (inputVector.range(7,0), inputVector(15, 8));
}

// Forward Declaration
class Packet;

template<int N>
ap_uint<N> byteSwap(ap_uint<N> inputVector) {
#pragma HLS inline
    // FIXME: static_assert
    if(N%8 != 1) assert("byteSwap not implemented.");
    ap_uint<N> t;
    for(int i = 0; i < N/8; i++) {
#pragma HLS unroll
        t(8*i+7, 8*i) = inputVector(N-1-8*i, N-8-8*i);
    }
    return t;
}

template<int N>
ap_uint<N> invert(ap_uint<N> x) {
#pragma HLS inline
    for(int i = 0; i < N; i ++) {
#pragma HLS unroll
        x[i] = ~x[i];
    }
    return x;
}

template <typename T>
class width_traits {
public:
    const static int WIDTH = T::WIDTH;
};
template <int D,int U,int TI,int TD>
class width_traits<ap_axis<D,U,TI,TD> > {
public:
    const static int WIDTH = D;
};
template <int D,int U,int TI,int TD>
class width_traits<ap_axiu<D,U,TI,TD> > {
public:
    const static int WIDTH = D;
};


struct axiWord {
    static const int WIDTH = 32;
	ap_uint<WIDTH>		data;
	ap_uint<WIDTH/8>		keep;
	ap_uint<1>		last;
	axiWord() {}
	axiWord(ap_uint<WIDTH> data, ap_uint<WIDTH/8> keep, ap_uint<1> last)
        : data(data), keep(keep), last(last) {}

};

static struct axiWord mask_invalid(struct axiWord t) {
    for (int i = 0; i < width_traits<struct axiWord>::WIDTH/8; i++) {
        if(!t.keep[i]) t.data(8 * i + 7, 8 * i) = 0;
    }
    return t;
}
static std::ostream & operator <<(std::ostream &stream, const struct axiWord &w) {
    stream << std::hex << std::noshowbase;
    stream << std::setfill('0');
    for(int i = axiWord::WIDTH/8; i > 0; i--) {
        stream << std::setw(2) << ((uint16_t) w.data(i*8-1, i*8-8));
    }
    stream << " " << std::setw(2) << ((uint32_t) w.keep) << " ";
    stream << std::setw(1) << ((uint32_t) w.last);
    return stream;
}
static std::istream & operator >>(std::istream &stream, struct axiWord &w) {
    uint16_t keepTemp;
    uint64_t dataTemp;
    uint16_t lastTemp;
    stream >> std::hex >> dataTemp >> keepTemp >> lastTemp;
    w.data = dataTemp;
    w.keep = keepTemp;
    w.last = lastTemp;
    return stream;
}
static bool operator ==(const struct axiWord &w1, const struct axiWord &w2) {
    bool match = true;
    for(int i = 0; i < axiWord::WIDTH/8; i++) {
        match &= !w1.keep[i] || (w1.data(8*i+7,8*i) == w2.data(8*i+7,8*i));
    }
    return match && (w1.keep == w2.keep) && (w1.last == w2.last);
}
static bool operator !=(const struct axiWord &w1, const struct axiWord &w2) {
    return !(w1 == w2);
}
// Checksum is compute in native byte order, not network byte order!
template <int N> class IPChecksum {
    const static int S = N / 16; // The number of parallel checksums on each word.
    ap_uint<16> val[S];
public:
    IPChecksum() {
#pragma HLS inline
#pragma HLS array_partition variable=val complete
        for (int i = 0; i < S; i++)
#pragma HLS unroll
            val[i] = 0;
    }
    IPChecksum(ap_uint<16> x) {
#pragma HLS inline
#pragma HLS array_partition variable=val complete
        val[0] = invert(x);
        for (int i = 1; i < S; i++)
#pragma HLS unroll
            val[i] = 0;
    }
    IPChecksum& add(ap_uint<16> x) {
#pragma HLS inline
        int i = 0;
        ap_uint<17> t = val[i] + x;
        val[i] = (t + (t >> 16)) & 0xFFFF;
        return (*this);
    }
    IPChecksum& add_data_network_byte_order(ap_uint<N> x) {
#pragma HLS inline
        for (int i = 0; i < S; i++) {
#pragma HLS unroll
            // 16-bit one's complement addition
            ap_uint<17> t = val[i] + byteSwap16(x(16 * i + 15, 16 * i));
            val[i] = (t + (t >> 16)) & 0xFFFF;
        }
        return (*this);
    }
    // IPChecksum& add(unsigned int x) {
    //     return add(ap_uint<N>(x));
    // }
    IPChecksum& subtract(ap_uint<16> x) {
#pragma HLS inline
        int i = 0;
        ap_uint<17> t = val[i] - x;
        val[i] = (t - (t >> 16)) & 0xFFFF;
        return (*this);
    }
    IPChecksum& subtract_data_network_byte_order(ap_uint<N> x) {
#pragma HLS inline
        for (int i = 0; i < S; i++) {
#pragma HLS unroll
            // 16-bit one's complement addition
            ap_uint<17> t = val[i] -byteSwap16( x(16 * i + 15, 16 * i));
            val[i] = (t - (t >> 16)) & 0xFFFF;
        }
        return (*this);
    }
    ap_uint<16> get() const {
#pragma HLS inline
        ap_uint<16 + BitWidth<S>::Value> t = val[0];
        for (int i = 1; i < S; i++) {
#pragma HLS unroll
            t += val[i];
        }
        t = (t + (t >> 16)) & 0xFFFF;
        return invert(t);
    }
};

template<int N>
ap_uint<BitWidth<N>::Value> keptbytes(ap_uint<N> keep) {
#ifndef __SYNTHESIS__
    for(int i = 1; i < N; i++) {
        if(keep[i] && !keep[i-1]) {
            std::cout << "Sparse keep not allowed: " << std::hex << keep << "\n."; assert(false);
        }
    }
#endif
    return invert(ap_uint<N+1>(keep)).reverse().countLeadingZeros();
}
// 0 -> 0b1111
// 1 -> 0b0001
// 2 -> 0b0011
// 3 -> 0b0111
template<int N>
void init_generatekeep_ROM(ap_uint<N> table[64]) {
#pragma HLS inline self off
    for(int i = 0; i < N; i++) {
        table[i] = (i == 0) ? -1 : (1<<i)-1;
    }
}
template<int N>
ap_uint<N> generatekeep(ap_uint<BitWidth<N>::Value> remainder) {
#pragma HLS inline
    ap_uint<N> table[64]; // FIXME: workaround for HLS bug.
    init_generatekeep_ROM(table);
    return table[remainder];
}
template<>
inline ap_uint<4> generatekeep<4>(ap_uint<BitWidth<4>::Value> remainder) {
#pragma HLS inline
    const ap_uint<4> table[64] = {0xF, 0x1, 0x3, 0x7}; // FIXME: workaround for HLS bug.
    return table[remainder];
}
template<>
inline ap_uint<8> generatekeep<8>(ap_uint<BitWidth<8>::Value> remainder) {
#pragma HLS inline
    const ap_uint<8> table[64] = {0xFF, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F}; // FIXME: workaround for HLS bug.
    return table[remainder];
}
template<>
inline ap_uint<16> generatekeep<16>(ap_uint<BitWidth<16>::Value> remainder) {
#pragma HLS inline
    const ap_uint<16> table[64] = {0xFFFF, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F,
                                   0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF}; // FIXME: workaround for HLS bug.
    return table[remainder];
}

// 0 -> 0b0000
// 1 -> 0b0001
// 2 -> 0b0011
// 3 -> 0b0111
template<int N>
void init_generatealign_ROM(ap_uint<N> table[64]) {
#pragma HLS inline self off
    for(int i = 0; i < N; i++) {
        table[i] = (1<<i)-1;
    }
}
template<int N>
ap_uint<N> generatealign(ap_uint<BitWidth<N>::Value> remainder) {
#pragma HLS inline
    ap_uint<N> table[64]; // FIXME: workaround for HLS bug.
    init_generatealign_ROM(table);
    return table[remainder];
    //  return (1<<rem)-1;
}

// 0 -> 0b0000
// 1 -> 0b1000
// 2 -> 0b1100
// 3 -> 0b1110
template<int N>
void init_keepFlag_ROM(ap_uint<N> table[64]) {
#pragma HLS inline self off
    for(int i = 0; i < N; i++) {
        table[i] = ap_uint<N>(-1) << (N-i);
        //        std::cout << "init_keepFlag_ROM["<<i<<"] = " << table[i].to_string(2) << "\n";
    }
}
template<int N>
ap_uint<N> keepFlag(ap_uint<BitWidth<N>::Value> remainder) {
#pragma HLS inline
    ap_uint<N> table[64]; // FIXME: workaround for HLS bug.
    init_keepFlag_ROM(table);
    return table[remainder];
}

template <typename PayloadT, int _LENGTH>
class header {
    int length;
public:
    const static int MAX_WIDTH=64; // Width in bytes of interfaces that can be serialized
    const static int LENGTH=_LENGTH;
    static const int NEXT = 0; // The offset of the first field of the header.
    //    static const int S = axiWord::WIDTH/8;

    unsigned char data[LENGTH+MAX_WIDTH];
    bool valid[LENGTH+MAX_WIDTH];
    PayloadT &p;
    IPChecksum<16> checksum;

    // If state is false, then we're writing to the local buffer
    // If state is true, then we're delegating storage to the payload.
    bool push_state;
    // The number of words pushed since the last clear.
    ap_uint<BitWidth<MTU>::Value> push_word;
    ap_uint<512> push_prevdata; // Max width = 512.
    ap_uint<512/8> push_prevkeep;

    //    PayloadT myp;

    //    header():length(LENGTH),push_state(false), push_word(0), push_prevkeep(0) { }

    header(PayloadT &payload):length(LENGTH),p(payload),push_state(false), push_word(0), push_prevkeep(0) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        //        const int S = axiWord::WIDTH/8;
#pragma HLS array_partition variable=data complete
#pragma HLS array_partition variable=valid complete
        /*
#pragma HLS array_partition variable=data cyclic factor=S
        if(S<32) {
#pragma HLS RESOURCE variable=data core=RAM_1P_LUTRAM
        }
        */
    }

    int data_length() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return length + p.data_length();
    }

    void clear() {
        push_state = false;
        push_word = 0;
        push_prevkeep = 0;
        p.clear();
    }
    bool haspushdatatoflush() {
        return push_prevkeep != 0 || p.haspushdatatoflush();
    }
    template <int N>
    bool push(ap_uint<N> indata, ap_uint<N/8> keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        // The number of data beats that will store into the local buffer
        const int LBEATS = (length + S - 1) / S;
        // One hot encoded flags for aligning data at the end of this header.
        // If the length of this header is evenly divisible by S, then this will be all ones.
        // In other cases it has ones in the beginning and zeros towards the end.
        /*const*/ ap_uint<S> alignflag = generatekeep<S>(length%S);
        // The number of ones in alignflag
        int localbytes = length%S == 0 ? S: length%S;

        if(!push_state) {
            // Shift data by length%S
            for (int i = 0; i < LBEATS*S-S; i++) {
                data[i] = data[i+S];
                valid[i] = valid[i+S];
                assert(i < LENGTH+MAX_WIDTH);
                assert(i+S < LENGTH+MAX_WIDTH);
                //  std::cout << "shift last data[" << std::dec << i << "] = " << std::hex << (int) data[i] << " " << valid[i] << "\n";
            }
        }

        bool storetodeferred = false;
        // A set of flags that indicate whether we're writing to the local buffer (true) or delegating the storage to the payload
        ap_uint<S> localflag = ap_uint<S>(-1);

        if(!push_state && (push_word == LBEATS-1)) {
            localflag = alignflag;
            push_state = true;
            // It's possible that no realignment is necessary
            // However, we still delay the storage for one data beat
            storetodeferred = false;
        } else if(push_state) {
            localflag = ap_uint<S>(0);
            storetodeferred = true;
        }
        // grab the leftover data from last time
        ap_uint<N> datatowrite = push_prevdata;
        ap_uint<S> keeptowrite = push_prevkeep;
        push_prevkeep = 0;
        for (int i = 0; i < S; i++) {
            //std::cout << "writing data[" << std::dec << (S * (word + wordoffset[i]) + i + byteoffset[i]) << "]\n";
            unsigned char c = indata(8 * i + 7, 8 * i);
            if(localflag[i]) {
                data[LBEATS*S-S+i] = c;
                assert(LBEATS*S-S+i < LENGTH+MAX_WIDTH);
            }
            if(!storetodeferred) {
                valid[LBEATS*S-S+i] = localflag[i];
                assert(LBEATS*S-S+i < LENGTH+MAX_WIDTH);
            }
            // realign the data for deferred storage
            if(alignflag[i]) {
                // assert(i+S-length%S>=0);
                // assert(i<length%S);
                // datatowrite(8*(i+S-length%S)+7, 8*(i+S-length%S) ) = c;
                // keeptowrite[i+S-length%S] = keep[i];
                assert(i+S-localbytes>=0);
                assert(i<localbytes);
                datatowrite(8*(i+S-localbytes)+7, 8*(i+S-localbytes) ) = c;
                keeptowrite[i+S-localbytes] = keep[i];
            } else {
                assert(i>=length%S);
                assert(i< S+length%S);
                push_prevdata(8*(i-length%S)+7, 8*(i-length%S) ) = c;
                push_prevkeep[i-length%S] = keep[i];
            }
        }
        //            std::cout << "prevkeep = " << std::hex << prevkeep << "\n";
        push_word++;
        //        bytesread += keptbytes(t.keep);
        if(storetodeferred) {
            // std::cout << "deferredwords = " << std::dec << deferredwords << "\n";
            // std::cout << "keptbytes = " << std::dec << keptbytes(keeptowrite) << "\n";
            p.extend(S*push_word+keptbytes(keeptowrite));
            return p.push(datatowrite, keeptowrite);
        }
        return (push_prevkeep == 0);
    }
    bool haspopdatatoflush() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return valid[0] != 0;
        /*      ap_uint<S> v;
        for(int i = 0; i < S; i++) {
            v[i] = valid[i];
        }
        return v != 0;*/
    }

    void pop_init() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < length; i++) {
#pragma HLS unroll
            valid[i] = true;
        }
        p.pop_init();
    }

    // If state is false, then we're writing to the local buffer
    // If state is true, then we're delegating storage to the payload.
    template <int N>
    void pop(ap_uint<N> &indata, ap_uint<N/8> &keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        //        assert(LENGTH > S);
        ap_uint<N> d;// = p.pop();
        ap_uint<S> k;// = -1;
        p.pop(d,k);
        // std::cout << "shifting in " << std::hex << (long long) d << " " << (int)k << "\n";
        for(int i = 0; i < S; i++) {
            // FIXME: don't read past end.
            if(length+i > 0) {
                data[length+i] = d(8*i+7, 8*i);
                valid[length+i] = k[i];
            }
        }
        int totalbytes = data_length();
        int LBEATS = (length + S - 1) / S;
        int BEATS = (totalbytes + S - 1) / S;
        for (int i = 0; i < S; i++) {
            indata(8 * i + 7, 8 * i) = data[i];
            keep[i] = valid[i];
        }
        // Shift data by length%S
        for (int i = 0; i < LENGTH; i++) {
            data[i] = data[i+S];
            valid[i] = valid[i+S];
            //std::cout << "shift last data[" << std::dec << i << "] = " << std::hex << (int) data[i] << "\n";
        }

        //        return d;
    }

    template<int N>
    ap_uint<8*N> get(int p) const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+N-1-i);
        }
        return t;
    }
    template<int N>
    // get little endian format
    // byte 0 in bits 7:0
    ap_uint<8*N> get_le(int p) {
#pragma HLS inline
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+i);
        }
        return t;
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(p+N-1-i, c);
        }
    }
    template<int N>
    // set little endian format
    // byte 0 in bits 7:0
    void set_le(int p, ap_uint<8*N> t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(i, c);
        }
    }

    // template<typename T>
    // typename T::FieldType get() {
    //     return get<T::FieldType::W>(T::OFFSET);
    // }
    // template<typename T>
    // typename T::FieldType set(ap_uint<T::FieldType::W> x) {
    //     return set<T::FieldType::W>(T::OFFSET, x);
    // }

    unsigned char get_byte(int j) const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return j < length ? data[j] : p.get_byte(j-length);
    }

    void set_byte(int j, unsigned char c) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        if(j < length) {
            data[j] = c;
            valid[j] = true;
        } else p.set_byte(j-length, c);
    }
    void extend(int j) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        //        assert(j > length);
        if(j > length) p.extend(j-length);
    }

    void verify_consistency() {
#ifndef __SYNTHESIS__
        for(int i = 0; i < length; i++) {
            //valid[i] = true;
             assert(valid[i]);
        }
        p.verify_consistency();
#endif
    }

 
    template <typename TBeat>
    void serialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = width_traits<TBeat>::WIDTH/8;
        int totalbytes = data_length();
        int LBEATS = (length + S - 1) / S;
        int BEATS = (totalbytes + S - 1) / S;
        // A set of flags that indicate whether we're writing to the local buffer (true) or delegating the storage to the payload
        ap_uint<S> localflag = ap_uint<S>(-1);
        // The number of full data beats written to deferred storage.
        ap_uint<BitWidth<MTU/S>::Value> deferredwords = 0;
        // If state is false, then we're writing to the local buffer
        // If state is true, then we're delegating storage to the payload.
        bool state = false;

        pop_init();
        verify_consistency();

        TBeat t;
        //#pragma HLS array_partition variable=p.data cyclic factor=4
        // for(int i = 0; i < length; i++) {
        //     valid[i] = true;
        // }
        bool notdone = true;
    SERIALIZE:
        while(notdone) {
#pragma HLS pipeline II=1
            ap_uint<width_traits<TBeat>::WIDTH> d;// = pop();
            ap_uint<width_traits<TBeat>::WIDTH/8> k;//= -1;
            pop(d,k);
            notdone = haspopdatatoflush();
            t.data = d;
            t.keep = k;
            t.last = !notdone;
            // std::cout << std::hex << "S:" << t.data << " " << t.keep << "\n";
            stream.write(t);
        }
#ifdef DEBUG
        std::cout << "serialized " << std::dec << totalbytes << " bytes.\n";
#endif
        // std::cout << "serialized " << std::dec << totalbytes << " bytes on " << stream.get_name() << ".\n";
    }

    template<int W>
    void serialize(ap_uint<W> *array, int &len) {
#if defined(__SYNTHESIS__)||!defined(OPTIMIZE_SERIALIZE)
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = W/8;

        pop_init();
        verify_consistency();

        //#pragma HLS array_partition variable=p.data cyclic factor=4
        // for(int i = 0; i < length; i++) {
        //     valid[i] = true;
        // }
        bool notdone = true;
        ap_uint<BitWidth<MTU/S>::Value> count = 0;

    SERIALIZE:
        while(notdone) {
#pragma HLS pipeline II=1
            ap_uint<W> d;// = pop();
            ap_uint<S> k;//= -1;
            pop(d,k);
            notdone = haspopdatatoflush();
            array[count++] = d;
            // std::cout << std::hex << "S:" << d << " " << k << "\n";
        }
        len = data_length();
#ifdef DEBUG
        //        std::cout << "serialized " << std::dec << totalbytes << " bytes.\n";
#endif
#else
        memcpy(array, data, length);
        unsigned char *ptr = (unsigned char *) array;
        ptr += length;
        p.serialize((ap_uint<W> *)ptr, len);
        len += length;
#endif
    }

    // Read data from the packet stream at the end of this packet.
    // Store the computed checksum.
    template <typename TBeat>
    void deserialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        // Compute the checksum of the data as we read it.
        IPChecksum<width_traits<TBeat>::WIDTH> csum;
        // The number of bytes in each data beat.
        const int S = width_traits<TBeat>::WIDTH/8;
        // The number of bytes read from the input
        ap_uint<BitWidth<MTU+S>::Value> bytesread = 0;

        TBeat t;
        t.last = false;
        clear();
        bool notdone = true;
    DESERIALIZE: while (!t.last || notdone) {
#pragma HLS pipeline II = 1
            //#pragma HLS inline all recursive
            if(!t.last) {
                t = stream.read();
                // std::cout << std::hex << "D:" << t.data << " " << t.keep << "\n";
                if (bytesread > MTU) {
                    std::cout << "MTU reached.\n";
                  //  break;
                }
                assert(t.keep == ap_uint<S>(-1) || t.last);
                csum.add_data_network_byte_order(t.data);
            } else {
                t.keep = 0;
            }
            bytesread += keptbytes(t.keep);
            push(t.data, t.keep);
            notdone = haspushdatatoflush();
        }
        checksum = csum.get();
#ifdef DEBUG
        std::cout << "deserialized " << std::dec << bytesread << " bytes.\n";
#endif
        //std::cout << "deserialized " << std::dec << bytesread << " bytes on " << stream.get_name() << ".\n";
        verify_consistency();
        //std::cout << "p.length = " << std::dec << p.data_length() << "\n";
    }

    // Read data from the packet stream at the end of this packet.
    // Store the computed checksum.
    template<int W>
    void deserialize(ap_uint<W> *array, int len) {
#if defined(__SYNTHESIS__)||!defined(OPTIMIZE_SERIALIZE)
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
        clear();
        bool notdone = true;
    DESERIALIZE: while (!last || notdone) {
#pragma HLS pipeline II = 1
            //#pragma HLS inline all recursive
            if(!last) {
                last = (bytesread + S) >= len;
                data = array[bytesread/S];
                ap_uint<BitWidth<S>::Value> end(len%S);
                keep = last ? generatekeep<S>(end) : ap_uint<S>(-1);
                // std::cout << std::hex << "D:" << data << " " << keep << "\n";
                if (bytesread > MTU) {
                    std::cout << "MTU reached.\n";
                  //  break;
                }
                assert(keep == ap_uint<S>(-1) || last);
                csum.add_data_network_byte_order(data);
            } else {
                keep = 0;
            }
            bytesread += keptbytes(keep);
            push(data, keep);
            notdone = haspushdatatoflush();
        }
        checksum = csum.get();
#ifdef DEBUG
        std::cout << "deserialized " << std::dec << bytesread << " bytes.\n";
#endif
        //std::cout << "deserialized " << std::dec << bytesread << " bytes on " << stream.get_name() << ".\n";
        verify_consistency();
        //std::cout << "p.length = " << std::dec << p.data_length() << "\n";
#else
        int valid_length = std::min(len, length);
        memcpy(data, array, valid_length);
        memset(valid, 1, valid_length);
        unsigned char *ptr = (unsigned char *) array;
        ptr += length;
        len -= length;
        if(len > 0) {
            p.deserialize((ap_uint<W> *)ptr, len);
        }
#endif
    }

    PayloadT& get_payload() const {
        return p;
    }
    void dump(std::ostream &out) {
#ifdef DEBUG
        for(int i = 0; i < length; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
            if(i%8 == 7) out << " ";
        }
        out << "\n";
        p.dump(out);
#endif
    }
};


template <int _LENGTH>
class generic_header {
    int length;
public:
    const static int MAX_WIDTH=64; // Width in bytes of interfaces that can be serialized
    const static int LENGTH=_LENGTH;
    unsigned char data[LENGTH+MAX_WIDTH];
    bool valid[LENGTH+MAX_WIDTH];

    // If state is false, then we're writing to the local buffer
    // If state is true, then we're delegating storage to the payload.
    bool push_state;
    // The number of words pushed since the last clear.
    ap_uint<BitWidth<MTU>::Value> push_word;
    ap_uint<512> push_prevdata; // Max width = 512.
    ap_uint<512/8> push_prevkeep;

    generic_header():length(LENGTH) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
#pragma HLS array_partition variable=data complete
#pragma HLS array_partition variable=valid complete
    }

    int data_length() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return length;
    }
    void clear() {
        push_state = false;
        push_word = 0;
        push_prevkeep = 0;
    }
    bool haspushdatatoflush() {
        return push_prevkeep != 0;
    }
    template <int N>
    bool push(ap_uint<N> indata, ap_uint<N/8> keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        // The number of data beats that will store into the local buffer
        const int LBEATS = (length + S - 1) / S;
        // One hot encoded flags for aligning data at the end of this header.
        // If the length of this header is evenly divisible by S, then this will be all ones.
        // In other cases it has ones in the beginning and zeros towards the end.
        /*const*/ ap_uint<S> alignflag = generatekeep<S>(length%S);
        // The number of ones in alignflag
        int localbytes = length%S == 0 ? S: length%S;

        if(!push_state) {
            // Shift data by length%S
            for (int i = 0; i < LBEATS*S-S; i++) {
                data[i] = data[i+S];
                valid[i] = valid[i+S];
                assert(i < LENGTH+MAX_WIDTH);
                assert(i+S < LENGTH+MAX_WIDTH);
                //  std::cout << "shift last data[" << std::dec << i << "] = " << std::hex << (int) data[i] << " " << valid[i] << "\n";
            }
        }

        bool storetodeferred = false;
        // A set of flags that indicate whether we're writing to the local buffer (true) or delegating the storage to the payload
        ap_uint<S> localflag = ap_uint<S>(-1);

        if(!push_state && (push_word == LBEATS-1)) {
            localflag = alignflag;
            push_state = true;
            // It's possible that no realignment is necessary
            // However, we still delay the storage for one data beat
            storetodeferred = false;
        } else if(push_state) {
            localflag = ap_uint<S>(0);
            storetodeferred = true;
        }
        // grab the leftover data from last time
        ap_uint<N> datatowrite = push_prevdata;
        ap_uint<S> keeptowrite = push_prevkeep;
        push_prevkeep = 0;
        for (int i = 0; i < S; i++) {
            //std::cout << "writing data[" << std::dec << (S * (word + wordoffset[i]) + i + byteoffset[i]) << "]\n";
            unsigned char c = indata(8 * i + 7, 8 * i);
            if(localflag[i]) {
                data[LBEATS*S-S+i] = c;
                assert(LBEATS*S-S+i < LENGTH+MAX_WIDTH);
            }
            if(!storetodeferred) {
                valid[LBEATS*S-S+i] = localflag[i];
                assert(LBEATS*S-S+i < LENGTH+MAX_WIDTH);
            }
            // realign the data for deferred storage
            if(alignflag[i]) {
                // assert(i+S-length%S>=0);
                // assert(i<length%S);
                // datatowrite(8*(i+S-length%S)+7, 8*(i+S-length%S) ) = c;
                // keeptowrite[i+S-length%S] = keep[i];
                assert(i+S-localbytes>=0);
                assert(i<localbytes);
                datatowrite(8*(i+S-localbytes)+7, 8*(i+S-localbytes) ) = c;
                keeptowrite[i+S-localbytes] = keep[i];
            } else {
                assert(i>=length%S);
                assert(i< S+length%S);
                push_prevdata(8*(i-length%S)+7, 8*(i-length%S) ) = c;
                push_prevkeep[i-length%S] = keep[i];
            }
        }
        //            std::cout << "prevkeep = " << std::hex << prevkeep << "\n";
        push_word++;
        //        bytesread += keptbytes(t.keep);
        return (push_prevkeep == 0);
    }

    bool haspopdatatoflush() {
        return valid[0] != 0;
        /*      ap_uint<S> v;
        for(int i = 0; i < S; i++) {
            v[i] = valid[i];
        }
        return v != 0;*/
    }

    void pop_init() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < length; i++) {
#pragma HLS unroll
            valid[i] = true;
        }
    }

    // If state is false, then we're writing to the local buffer
    // If state is true, then we're delegating storage to the payload.
    template <int N>
    void pop(ap_uint<N> &indata, ap_uint<N/8> &keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        for(int i = 0; i < S; i++) {
            // FIXME: don't read past end.
            if(length+i > 0) {
                data[length+i] = 0;
                valid[length+i] = false;
            }
        }

        int totalbytes = data_length();
        int LBEATS = (length + S - 1) / S;
        int BEATS = (totalbytes + S - 1) / S;
        for (int i = 0; i < S; i++) {
            indata(8 * i + 7, 8 * i) = data[i];
            keep[i] = valid[i];
        }
        // Shift data by length%S
        for (int i = 0; i < LENGTH; i++) {
            data[i] = data[i+S];
            valid[i] = valid[i+S];
            //std::cout << "shift last data[" << std::dec << i << "] = " << std::hex << (int) data[i] << "\n";
        }

        //        return d;
    }

    template<int N>
    ap_uint<8*N> get(int p) const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+N-1-i);
        }
        return t;
    }
    template<int N>
    // get little endian format
    // byte 0 in bits 7:0
    ap_uint<8*N> get_le(int p) {
#pragma HLS inline
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+i);
        }
        return t;
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(p+N-1-i, c);
        }
    }
    template<int N>
    // set little endian format
    // byte 0 in bits 7:0
    void set_le(int p, ap_uint<8*N> t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(i, c);
        }
    }


    unsigned char get_byte(int j) const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return j < length ? data[j] : 0;
    }

    void set_byte(int j, unsigned char c) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        if(j < length) {
            data[j] = c;
        }
    }
    void extend(int j) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
    }

    void verify_consistency() {
#ifndef __SYNTHESIS__
        for(int i = 0; i < length; i++) {
            //valid[i] = true;
             assert(valid[i]);
        }
#endif
    }

    void dump(std::ostream &out) {
#ifdef DEBUG
        for(int i = 0; i < length; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
            if(i%8 == 7) out << " ";
        }
        out << "\n";
#endif
    }
};


class MinimalPacket {
  public:
    static const int max_length = 64;
    unsigned char data[64];
    ap_uint<BitWidth<MTU>::Value> length; // length in bytes;
    IPChecksum<16> checksum;
    // The number of words pushed since the last clear.
    ap_uint<BitWidth<MTU>::Value> push_word2;
    ap_uint<BitWidth<MTU>::Value> pop_word;
    bool pop_state;
    MinimalPacket():length(0),checksum() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        //        const int S = width_traits<TBeat>::WIDTH/8;
//         const int S = axiWord::WIDTH/8;
//         if(S<32) {
// #pragma HLS RESOURCE variable=data core=RAM_1P_LUTRAM
//         }
// #pragma HLS array_partition variable=data cyclic factor=S
    }
    int data_length() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return length;
    }
    unsigned char get_byte(int j) const {
#pragma HLS inline
        return data[j];
    }
    void set_byte(int j, unsigned char c) {
#pragma HLS inline
        data[j] = c;
    }
    void extend(int j) {
        length = j;
    }
    void verify_consistency() {
    }
    // Get an N-byte field at byte offset p, converting to machine order
    template<int N>
    ap_uint<8*N> get(int p) const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+N-1-i);
        }
        return t;
    }
    template<int N>
    // get little endian format
    // byte 0 in bits 7:0
    ap_uint<8*N> get_le(int p) {
#pragma HLS inline
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+i);
        }
        return t;
    }
    // Set an N-byte field at byte offset p, converting to machine order
    template<int N>
    void set(int p, ap_uint<8*N> t) {
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(p+N-1-i, c);
        }
    }
    void clear() {
        push_word2 = 0;
        pop_word = 0;
        pop_state = false;
    }
    bool haspushdatatoflush() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return false;
    }
    template<int N>
    bool push(ap_uint<N> d, ap_uint<N/8> valid) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        if(S<32) {
#pragma HLS RESOURCE variable=data core=RAM_1P_LUTRAM
        }
#pragma HLS array_partition variable=data cyclic factor=S
        for (int i = 0; i < S; i++) {
            // Is this conditional really necessary?
            unsigned char c = d(8 * i + 7, 8 * i);
            if(valid[i]) {
                //  std::cout << "writing deferred[" << std::dec << (S * push_word + i) << "] = " << std::hex << (int)c << "\n";
                set_byte(S*push_word2+i, c);
            }
        }
        extend(S*push_word2 + keptbytes(valid));
        push_word2++;
        return false;
    }
    void pop_init() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        pop_word = 0;
        pop_state = false;
    }
    template<int N>
    void pop(ap_uint<N> &indata, ap_uint<N/8> &keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        if(S<32) {
#pragma HLS RESOURCE variable=data core=RAM_1P_LUTRAM
        }
#pragma HLS array_partition variable=data cyclic factor=S
        keep = ap_uint<S>(-1);
        int LBEATS = (length + S - 1) / S;
        if(!pop_state && (pop_word >= LBEATS-1)) {
            keep = generatekeep<S>(length%S);
            pop_state = true;
        } else if(pop_state) {
            keep = ap_uint<S>(0);
        }
        for (int i = 0; i < S; i++) {
            // Is this conditional really necessary?
            unsigned char c = get_byte(S*pop_word+i);
            indata(8 * i + 7, 8 * i) = c;
        }
        pop_word++;
    }
    void dump(std::ostream &out) {
#ifdef DEBUG
        out << "packet: ";
        for(int i = 0; i < length; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
            if(i%8 == 7) out << " ";
        }
        out << "\n";
#endif
    }
};
class ZeroPadding {
  public:
    //    static const int S = axiWord::WIDTH/8;
    ap_uint<BitWidth<MTU>::Value> length; // length in bytes;
    ap_uint<BitWidth<MTU>::Value> pop_word3;
    bool pop_state;
    ZeroPadding():length(0), pop_word3(0), pop_state(false) {
#ifdef WORKAROUND
#pragma HLS inline
#endif

    }
    int data_length() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return length;
    }
    unsigned char get_byte(int j) const {
#pragma HLS inline
        return 0;
    }
    void set_byte(int j, unsigned char c) {
#pragma HLS inline
    }
    template<int N>
    ap_uint<8*N> get(int p) const {
        return 0;
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
    }
     void extend(int j) {
        length = j;
    }
    void verify_consistency() {
    }
    void clear() {
        pop_word3 = 0;
        pop_state = false;
    }
    bool haspopdatatoflush() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return false;
    }
    void pop_init() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        pop_word3 = 0;
        pop_state = (length == 0); // This allows the header length to be exactly equal to the packet size.  Is it worth it?
    }
    template<int N>
    void pop(ap_uint<N> &indata, ap_uint<N/8> &keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
        keep = ap_uint<S>(-1);
        int LBEATS = (length + S - 1) / S;
        if(!pop_state && (pop_word3 >= LBEATS-1)) {
            keep = generatekeep<S>(length%S);
            pop_state = true;
        } else if(pop_state) {
            keep = ap_uint<S>(0);
        }
        for (int i = 0; i < S; i++) {
            // Is this conditional really necessary?
            unsigned char c = get_byte(S*pop_word3+i);
            indata(8 * i + 7, 8 * i) = c;
        }
        pop_word3++;
    }
    template <typename TBeat>
     void serialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = width_traits<TBeat>::WIDTH / 8;
        int BEATS = (length + S - 1) / S;
        // partition data into S banks.
        // construct a word;
        TBeat t;

    SERIALIZE:
        for (int j = 0; j < BEATS; j++) {
#pragma HLS pipeline II=1
            t.data = 0;
            t.keep = (j == BEATS-1) ? generatekeep<S>(length%S) : ap_uint<S>(-1);
            t.last = (j == BEATS-1);
            //std::cout << std::hex << "S:" << t.data << " " << t.keep << "\n";
            stream.write(t);
        }
#ifdef DEBUG
        std::cout << "serialized " << std::dec << length << " bytes.\n";
#endif
        //std::cout << "serialized " << std::dec << length << " bytes on " << stream.get_name() << ".\n";
    }
   template<int W>
    void serialize(ap_uint<W> *array, int &len) {
#if defined(__SYNTHESIS__)||!defined(OPTIMIZE_SERIALIZE)
 #ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = W/8;
        pop_init();
        //#pragma HLS array_partition variable=p.data cyclic factor=4
        // for(int i = 0; i < length; i++) {
        //     valid[i] = true;
        // }
        bool notdone = true;
        ap_uint<BitWidth<MTU/S>::Value> count = 0;

    SERIALIZE:
        while(notdone) {
#pragma HLS pipeline II=1
            ap_uint<W> d;// = pop();
            ap_uint<S> k;//= -1;
            pop(d,k);
            notdone = haspopdatatoflush();
            array[count++] = d;
            //std::cout << std::hex << "S:" << d << " " << k << "\n";
        }
        len = data_length();
#ifdef DEBUG
        //        std::cout << "serialized " << std::dec << totalbytes << " bytes.\n";
#endif
#else
        memset(array, 0, length);
        len = length;
#endif
    }
};
class Packet {
public:
    //    static const int S = axiWord::WIDTH/8;
    ap_uint<BitWidth<MTU>::Value> length; // length in bytes;
    IPChecksum<16> checksum;
     // The number of words pushed since the last clear.
    ap_uint<BitWidth<MTU>::Value> push_word3;
    ap_uint<BitWidth<MTU>::Value> pop_word3;
    bool pop_state;
    unsigned char data[MTU];
    //class ipv4_hdr ipheader;

    Packet():length(0), checksum(), push_word3(0), pop_word3(0), pop_state(false) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
//         const int S = axiWord::WIDTH/8;
// #pragma HLS array_reshape variable=data cyclic factor=S
    }

    int data_length() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return length;
    }
    unsigned char get_byte(int j) const {
#pragma HLS inline
        return data[j];
    }
    void set_byte(int j, unsigned char c) {
#pragma HLS inline
        data[j] = c;
    }
    template<int N>
    ap_uint<8*N> get(int p) const {
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+N-1-i);
        }
        return t;
    }
    template<int N>
    // get little endian format
    // byte 0 in bits 7:0
    ap_uint<8*N> get_le(int p) {
#pragma HLS inline
        ap_uint<8*N> t;
        for(int i = 0; i < N; i++) {
            #pragma HLS unroll
            t(8*i+7, 8*i) = get_byte(p+i);
        }
        return t;
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
        for(int i = 0; i < N; i++) {
#pragma HLS unroll
            unsigned char c = t(8*i+7, 8*i);
            set_byte(p+N-1-i, c);
        }
    }
    void extend(int j) {
        length = j;
    }
    void verify_consistency() {
    }
    void clear() {
        push_word3 = 0;
        pop_word3 = 0;
        pop_state = false;
    }
    bool haspopdatatoflush() {
        return !pop_state;
    }
    bool haspushdatatoflush() {
        return false;
    }
    template<int N>
    bool push(ap_uint<N> d, ap_uint<N/8> valid) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = N/8;
#pragma HLS array_reshape variable=data cyclic factor=S

        for (int i = 0; i < S; i++) {
            // Is this conditional really necessary?  It require Byte-Write-Enable.
//            if(valid[i]) {
                // std::cout << "writing deferred[" << std::dec << (S * deferredwords + i) << "] = " << std::hex << (int)datatowrite[i] << "\n";
                set_byte(S * push_word3 + i, d(8 * i + 7, 8 * i));
  //          }
        }
        extend(S*push_word3 + keptbytes(valid));
        push_word3++;
        return false;
    }
    void pop_init() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        pop_word3 = 0;
        pop_state = (length == 0); // This allows the header length to be exactly equal to the packet size.  Is it worth it?
    }
    template<int N>
    void pop(ap_uint<N> &indata, ap_uint<N/8> &keep) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        #pragma HLS inline all recursive
        const int S = N/8;
#pragma HLS array_reshape variable=data cyclic factor=S
        keep = ap_uint<S>(-1);
        int LBEATS = (length + S - 1) / S;
        if(!pop_state && (pop_word3 >= LBEATS-1)) {
            keep = generatekeep<S>(length%S);
            pop_state = true;
        } else if(pop_state) {
            keep = ap_uint<S>(0);
        }
        for (int i = 0; i < S; i++) {
            // Is this conditional really necessary?
            unsigned char c = get_byte(S*pop_word3+i);
            indata(8 * i + 7, 8 * i) = c;
        }
        pop_word3++;
    }
    template <typename TBeat>
     void serialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = width_traits<TBeat>::WIDTH / 8;
        int BEATS = (length + S - 1) / S;
        // partition data into S banks.
        // construct a word;

    SERIALIZE:
        for (int j = 0; j < BEATS; j++) {
#pragma HLS pipeline II=1
            TBeat t;
            for (int i = 0; i < S; i++) {
                t.data(8 * i + 7, 8 * i) = data[S * j + i];
            }
            t.keep = (j == BEATS-1) ? generatekeep<S>(length%S) : ap_uint<S>(-1);
            t.last = (j == BEATS-1);
            //std::cout << std::hex << "S:" << t.data << " " << t.keep << "\n";
            stream.write(t);
        }
#ifdef DEBUG
        std::cout << "serialized " << std::dec << length << " bytes.\n";
#endif
        //std::cout << "serialized " << std::dec << length << " bytes on " << stream.get_name() << ".\n";
    }
   template<int W>
    void serialize(ap_uint<W> *array, int &len) {
#if defined(__SYNTHESIS__)||!defined(OPTIMIZE_SERIALIZE)
 #ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = W/8;
        pop_init();
        //#pragma HLS array_partition variable=p.data cyclic factor=4
        // for(int i = 0; i < length; i++) {
        //     valid[i] = true;
        // }
        bool notdone = true;
        ap_uint<BitWidth<MTU/S>::Value> count = 0;

    SERIALIZE:
        while(notdone) {
#pragma HLS pipeline II=1
            ap_uint<W> d;// = pop();
            ap_uint<S> k;//= -1;
            pop(d,k);
            notdone = haspopdatatoflush();
            array[count++] = d;
            //std::cout << std::hex << "S:" << d << " " << k << "\n";
        }
        len = data_length();
#ifdef DEBUG
        //        std::cout << "serialized " << std::dec << totalbytes << " bytes.\n";
#endif
#else
        memcpy(array, data, length);
        len = length;
#endif
    }
    
    // Read data from the packet stream at the end of this packet.
    // Store the computed checksum.
    template <typename TBeat>
    void deserialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        // Compute the checksum of the data as we read it.
        IPChecksum<width_traits<TBeat>::WIDTH> csum;
        // The number of bytes in each data beat.
        const int S = width_traits<TBeat>::WIDTH/8;
        // The number of bytes read from the input
        ap_uint<BitWidth<MTU+S>::Value> bytesread = 0;

        TBeat t;
        t.last = false;
        clear();
        bool notdone = true;
    DESERIALIZE: while (!t.last || notdone) {
#pragma HLS pipeline II = 1
            //#pragma HLS inline all recursive
            if(!t.last) {
                t = stream.read();
                // std::cout << std::hex << "D:" << t.data << " " << t.keep << "\n";
                if (bytesread > MTU) {
                    std::cout << "MTU reached.\n";
                  //  break;
                }
                assert(t.keep == ap_uint<S>(-1) || t.last);
                csum.add_data_network_byte_order(t.data);
            } else {
                t.keep = 0;
            }
            bytesread += keptbytes(t.keep);
            push(t.data, t.keep);
            notdone = haspushdatatoflush();
        }
        checksum = csum.get();
#ifdef DEBUG
        std::cout << "deserialized " << std::dec << bytesread << " bytes.\n";
#endif
        //std::cout << "deserialized " << std::dec << bytesread << " bytes on " << stream.get_name() << ".\n";
        verify_consistency();
        //std::cout << "p.length = " << std::dec << p.data_length() << "\n";
    }
    /*
    // Read data from the packet stream at the end of this packet.
    // Return the byte position of the final write.
    // Store the computed checksum.
    template <typename TBeat>
    void deserialize(hls::stream<TBeat> &stream) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        const int S = width_traits<TBeat>::WIDTH / 8;
        IPChecksum<width_traits<TBeat>::WIDTH> csum;
        // partition p into S banks.
        // construct a word;
        TBeat t;
        int offset = 0;
        int byteoffset[S];
        // FIXME: turn wordoffset into a ROM.
        int wordoffset[S];
        for (int i = 0; i < S; i++) {
            byteoffset[i] = (offset + i) % S;
            if (byteoffset[i] > i)
                wordoffset[i] = 1;
            else
                wordoffset[i] = 0;
            byteoffset[i] = 0;
            wordoffset[i] = 0;
        }
        // If bytepos == 0, we write:
        // P[0] P[1] P[2] P[3]
        // P[4] P[5] P[6] P[7]
        // If bytepos == 1, we write:
        // OLD  P[0] P[1] P[2]
        // P[3] P[4] P[5] P[6]
        // P[7]
        // If bytepos == 2, we output:
        // OLD  OLD  P[0] P[1]
        // P[2] P[3] P[4] P[5]
        // P[6] P[7]
        t.last = false;
        ap_uint<BitWidth<MTU/S>::Value> word = length / S;
    DESERIALIZE:
        while (!t.last) {
#pragma HLS pipeline II = 1
            t = stream.read();
            //std::cout << std::hex << "D:" << t.data << " " << t.keep << "\n";
            if (length < MTU) {
                assert(t.keep == ap_uint<S>(-1) || t.last);
                csum.add_data_network_byte_order(t.data);
                for (int i = 0; i < S; i++) {
                    //                    if (t.keep[i]) {
                        //std::cout << "writing data[" << std::dec << (S * (word + wordoffset[i]) + i + byteoffset[i]) << "]\n";
                        data[S * (word + wordoffset[i]) + i + byteoffset[i]] =
                            t.data(8 * i + 7, 8 * i);
                        //}
                }
                length += keptbytes(t.keep);
            } else {
                std::cout << "MTU reached.\n";
                break;
            }
            word++;
        }
        checksum = csum.get();
#ifdef DEBUG
        std::cout << "deserialized " << std::dec << length << " bytes.\n";
#endif
        //std::cout << "deserialized " << std::dec << length << " bytes on " << stream.get_name() << ".\n";
    }
    */
    // Read data from the packet stream at the end of this packet.
    // Store the computed checksum.
    template<int W>
    void deserialize(ap_uint<W> *array, int len) {
#if defined(__SYNTHESIS__)||!defined(OPTIMIZE_SERIALIZE)
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
        clear();
        bool notdone = true;
    DESERIALIZE: while (!last || notdone) {
#pragma HLS pipeline II = 1
            //#pragma HLS inline all recursive
            if(!last) {
                last = (bytesread + S) >= len;
                data = array[bytesread/S];
                ap_uint<BitWidth<S>::Value> end(len%S);
                keep = last ? generatekeep<S>(end) : ap_uint<S>(-1);
                // std::cout << std::hex << "D:" << data << " " << keep << "\n";
                if (bytesread > MTU) {
                    std::cout << "MTU reached.\n";
                  //  break;
                }
                assert(keep == ap_uint<S>(-1) || last);
                csum.add_data_network_byte_order(data);
            } else {
                keep = 0;
            }
            bytesread += keptbytes(keep);
            push(data, keep);
            notdone = haspushdatatoflush();
        }
        checksum = csum.get();
#ifdef DEBUG
        std::cout << "deserialized " << std::dec << bytesread << " bytes.\n";
#endif
        //std::cout << "deserialized " << std::dec << bytesread << " bytes on " << stream.get_name() << ".\n";
        verify_consistency();
        //std::cout << "p.length = " << std::dec << p.data_length() << "\n";
#else
        memcpy(data, array, len);
        length = len;
#endif
    }
   void dump(std::ostream &out) {
#ifdef DEBUG
       out << "packet: ";
        for(int i = 0; i < length; i++) {
            out << std::hex << std::setfill('0') << std::setw(2)<< (int)data[i];
            if(i%8 == 7) out << " ";
        }
        out << "\n";
#endif
   }
};

class FIELD_LE {};
class FIELD_BE {};
template <typename T, typename PreviousField, typename HeaderT, typename ENDIAN=FIELD_BE>
class field { };

// Metafunction that returns the length of a field.
template< typename T1 > struct Mfield_length
{
    typedef boost::mpl::int_<T1::LENGTH> type;
};

typedef boost::mpl::lambda<boost::mpl::plus<boost::mpl::_1, Mfield_length<boost::mpl::_2> > >::type Maccum_length;

typedef boost::mpl::lambda<boost::mpl::fold<boost::mpl::_,
                                            boost::mpl::int_<0>,
                                            Maccum_length> >::type Mfind_header_length;


template <typename PayloadT, typename _Fields>
class compound_header : public header<PayloadT, boost::mpl::apply<Mfind_header_length, _Fields>::type::value> {
public:
    typedef _Fields Fields;
    typedef header<PayloadT, boost::mpl::apply<Mfind_header_length, Fields>::type::value> HeaderT;
    compound_header(PayloadT &payload):HeaderT(payload) { }

    template<typename Field>
    typename Field::FieldType get() const {
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        return HeaderT::template get<Field::LENGTH>(start);

    }
    template<typename Field>
    void set(typename Field::FieldType t) {
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        HeaderT::template set<Field::LENGTH>(start, t);
    }
    using HeaderT::get_le;
    using HeaderT::set_le;
    using HeaderT::get;
    using HeaderT::set;
};

template <typename BackT>
class remainder_header {
public:
    BackT &h;
    int offset;
    remainder_header(BackT &backing_packet, int _offset):h(backing_packet),offset(_offset) { }
    int data_length() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.data_length() - offset;
    }

    void extend(int j) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.extend(j + offset);
    }

    template<int N>
    ap_uint<8*N> get(int p) const {
        return h.template get<N>(p+offset);
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
        return h.template set<N>(p+offset,t);
    }
};

template <typename BackT>
remainder_header<BackT> skip(BackT &p, int x) {
    return remainder_header<BackT>(p, x);
}

template <typename BackT, typename _Fields>
class parsed_header {// : public header<BackT, boost::mpl::apply<Mfind_header_length, _Fields>::type::value> {
public:
    static const int LENGTH = boost::mpl::apply<Mfind_header_length, _Fields>::type::value;
    typedef _Fields Fields;
    BackT &h;
    remainder_header<BackT> p;
    //    int start;

    parsed_header(BackT &backing_packet):h(backing_packet),p(backing_packet,LENGTH) { }
    //   parsed_header(BackT &backing_packet, int _start):h(backing_packet),start(_start),p(backing_packet,LENGTH+_start) { }

    int data_length() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.data_length();
    }

    void extend(int j) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.extend(j);
    }

    template<int N>
    ap_uint<8*N> get(int p) const {
        return h.template get<N>(p);
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
        return h.template set<N>(p,t);
    }

    template<typename Field>
    typename Field::FieldType get() const {
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        return get<Field::LENGTH>(start);

    }
    template<typename Field>
    void set(typename Field::FieldType t) {
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        set<Field::LENGTH>(start, t);
    }
};

template <typename BackT>
class optional_remainder_header {
public:
    BackT &h;
    bool condition;
    int offset1;
    int offset2;
    optional_remainder_header(BackT &backing_packet,
                              bool _condition,
                              int _offset1,
                              int _offset2):
        h(backing_packet),
        condition(_condition),
        offset1(_offset1),
        offset2(_offset2) { }
    int data_length() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.data_length() - (condition ? offset1 : offset2);
    }

    void extend(int j) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.extend(j + (condition ? offset1 : offset2));
    }

    template<int N>
    ap_uint<8*N> get(int p) const {
        if(condition) {
            return h.template get<N>(p+offset1);
        } else {
            return h.template get<N>(p+offset2);
        }
    }
    template<int N>
    void set(int p, ap_uint<8*N> t) {
        if(condition) {
            return h.template set<N>(p+offset1,t);
        } else {
            return h.template set<N>(p+offset2,t);
        }
    }
};

template <typename T, typename Name>
class newfield {
public:
    // FIXME: model Little Endian.
    typedef T FieldType;
    static const int LENGTH = Type_BitWidth<T>::Value/8;//sizeof(T);
};

template <typename T, typename PreviousField, typename HeaderT>
class field<T, PreviousField, HeaderT, FIELD_BE> {
public:
    typedef T FieldType;
    static const int OFFSET = PreviousField::NEXT;
    static const int LENGTH = Type_BitWidth<T>::Value/8;//sizeof(T);
    static const int NEXT = OFFSET + LENGTH;
    HeaderT &h;
    field(HeaderT &_h):h(_h) { }
    field(HeaderT &_h, T def):h(_h) { h.template set<LENGTH>(OFFSET, def); }
    T get() const {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.template get<LENGTH>(OFFSET);
    }
    void set(T t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        h.template set<LENGTH>(OFFSET, t);
    }
    template<typename H>
    static T get(H h) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return h.template get<LENGTH>(OFFSET);
    }
    template<typename H>
    static void set(H h, T t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        h.template set<LENGTH>(OFFSET, t);
    }
};
template <typename T, typename PreviousField, typename HeaderT>
class field<T, PreviousField, HeaderT, FIELD_LE> {
public:
    typedef T FieldType;
    static const int OFFSET = PreviousField::NEXT;
    static const int LENGTH = Type_BitWidth<T>::Value/8;//sizeof(T);
    static const int NEXT = OFFSET + LENGTH;
    HeaderT &h;
    field(HeaderT &_h):h(_h) { }
    field(HeaderT &_h, T def):h(_h) { h.template set<LENGTH>(OFFSET, def); }
    T get() {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return byteSwap(h.template get<LENGTH>(OFFSET));
    }
    void set(T t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        h.template set<LENGTH>(OFFSET,  byteSwap(t));
    }
    template<typename H>
    static T get(H h) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        return byteSwap(h.template get<LENGTH>(OFFSET));
    }
    template<typename H>
    static void set(H h, T t) {
#ifdef WORKAROUND
#pragma HLS inline
#endif
        h.template set<LENGTH>(OFFSET,  byteSwap(t));
    }
};

template <typename _Fields>
class fixed_header : public generic_header<boost::mpl::apply<Mfind_header_length, _Fields>::type::value> {
public:
    typedef _Fields Fields;
    typedef generic_header<boost::mpl::apply<Mfind_header_length, Fields>::type::value> HeaderT;
    fixed_header() {
#pragma HLS inline
    }

    template <typename PayloadT>
    using parsed_hdr = parsed_header<PayloadT, Fields>;
    template <typename T>
    static parsed_hdr<T> parse_hdr(T &h) { return parsed_hdr<T>(h); }

    template <typename PayloadT>
    using compound_hdr = compound_header<PayloadT, Fields>;

    template<typename PayloadT>
    static compound_hdr<PayloadT> contains(PayloadT &p) { compound_hdr<PayloadT> h(p); return h; }

    static fixed_header<_Fields> contains() { fixed_header<_Fields> h; return h; }
    
    using HeaderT::get;
    using HeaderT::set;
    template<typename Field>
    typename Field::FieldType get() const {
#pragma HLS inline
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        return HeaderT::template get<Field::LENGTH>(start);

    }
    template<typename Field>
    void set(typename Field::FieldType t) {
#pragma HLS inline
        BOOST_MPL_ASSERT_MSG((boost::mpl::not_<boost::is_same<typename boost::mpl::end<Fields>::type, typename boost::mpl::find<Fields, Field>::type> >::value),
                             ATTEMPT_TO_ACCESS_FIELD_NOT_IN_HEADER, (Field));
        typedef typename boost::mpl::iterator_range<typename boost::mpl::begin<Fields>::type,
                                                    typename boost::mpl::find<Fields, Field>::type
                                           >::type pred_Fields;
        const int start = boost::mpl::apply<Mfind_header_length, pred_Fields>::type::value;
        HeaderT::template set<Field::LENGTH>(start, t);
    }
};


template <typename PayloadT>
class ethernet_hdr : public header<PayloadT, 14> {
public:
    struct ethernet_etherType {
        static const unsigned short IPV4 = 0x0800;
        static const unsigned short ARP  = 0x0806;
        static const unsigned short VLAN = 0x8100;
    };
    struct arp_opCode{
        static const unsigned short REQUEST = 1;
        static const unsigned short REPLY  = 2;
    };
    typedef header<PayloadT, 14> HeaderT;
    // ethernet_hdr():HeaderT(),
    //                                 destinationMAC(*this),
    //                                 sourceMAC(*this),
    //                                 etherType(*this) { }
    ethernet_hdr(PayloadT &payload):HeaderT(payload),
                                    destinationMAC(*this),
                                    sourceMAC(*this),
                                    etherType(*this) { }

    using HeaderT::get_payload;
    using HeaderT::get;
    using HeaderT::set;
    
    typedef field<ap_uint<48>, HeaderT, HeaderT> destinationMACT;
    typedef field<ap_uint<48>, destinationMACT, HeaderT> sourceMACT;
    typedef field<ap_uint<16>, sourceMACT, HeaderT> etherTypeT;

    destinationMACT destinationMAC;
    sourceMACT sourceMAC;
    etherTypeT etherType;

    void dump(std::ostream &out) {
#ifdef DEBUG
        out << "ether : ";
        HeaderT::dump(out);
#endif
    }
};

namespace ethernet {
    struct ethernet_etherType {
        static const unsigned short IPV4 = 0x0800;
        static const unsigned short ARP  = 0x0806;
        static const unsigned short VLAN = 0x8100;
    };
    typedef newfield<ap_uint<48>, boost::mpl::string<'dmac'> > destinationMAC;
    typedef newfield<ap_uint<48>, boost::mpl::string<'smac'> > sourceMAC;
    typedef newfield<ap_uint<16>, boost::mpl::string<'type'> > etherType;

    using header = fixed_header<boost::mpl::vector<destinationMAC, sourceMAC, etherType> >;
    template <typename T> header::parsed_hdr<T> parse_ethernet_hdr(T &h) { return header::parsed_hdr<T>(h); }
}



template <typename PayloadT>
class arp_hdr : public header<PayloadT, 28> {
public:

    typedef header<PayloadT, 28> HeaderT;
    static const int REQUEST = 1;
    static const int REPLY = 2;
    // arp_hdr():HeaderT(),
    //                            hwtype(*this,1),
    //                            ptype(*this, ethernet_hdr<Packet>::ethernet_etherType::IPV4),
    //                            hwlen(*this, 6),
    //                            plen(*this, 4),
    //                            op(*this, REQUEST),
    //                            hwsrc(*this),
    //                            psrc(*this),
    //                            hwdst(*this),
    //                            pdst(*this) { }
    arp_hdr(PayloadT &payload):HeaderT(payload),
                               hwtype(*this,1),
                               ptype(*this, ethernet_hdr<Packet>::ethernet_etherType::IPV4),
                               hwlen(*this, 6),
                               plen(*this, 4),
                               op(*this, REQUEST),
                               hwsrc(*this),
                               psrc(*this),
                               hwdst(*this),
                               pdst(*this) { }

    using HeaderT::get_payload;
    using HeaderT::get;
    using HeaderT::set;

    typedef field<ap_uint<16>, HeaderT, HeaderT> hwtypeT;
    typedef field<ap_uint<16>, hwtypeT, HeaderT> ptypeT;
    typedef field<ap_uint<8>, ptypeT, HeaderT> hwlenT;
    typedef field<ap_uint<8>, hwlenT, HeaderT> plenT;
    typedef field<ap_uint<16>, plenT, HeaderT> opT;
    typedef field<ap_uint<8*6>, opT, HeaderT> hwsrcT;
    typedef field<ap_uint<8*4>, hwsrcT, HeaderT> psrcT;
    typedef field<ap_uint<8*6>, psrcT, HeaderT> hwdstT;
    typedef field<ap_uint<8*4>, hwdstT, HeaderT> pdstT;

    hwtypeT hwtype;
    ptypeT ptype;
    hwlenT hwlen;
    plenT plen;
    opT op;
    hwsrcT hwsrc;
    psrcT psrc;
    hwdstT hwdst;
    pdstT pdst;

    // // Read data from the packet stream at the end of this packet.
    // // Store the computed checksum.
    // template<typename T, int N>
    // arp_hdr<header<T, N-LENGTH> > parse(header<T, N> h) {
    //     BOOST_MPL_ASSERT_RELATION(N, >, LENGTH);
    //     const static int remainderLength = N-LENGTH;
    //     Header<T, N-LENGTH> remainder(p);
    //     for(int i = 0; i < N; i++) {
    //         set<1>(i, h.get<1>(i));
    //     }
    //     for(int i = 0; i < remainderLength; i++) {
    //         remainder.set<1>(i, h.get<1>(i+N));
    //     }
    // }

};

namespace arp {
    struct arp_opCode{
        static const unsigned short REQUEST = 1;
        static const unsigned short REPLY  = 2;
    };
    typedef newfield<ap_uint<16>,  boost::mpl::string<'hwty','pe'> > hwtype;
    typedef newfield<ap_uint<16>,  boost::mpl::string<'ptyp','e'> > ptype;
    typedef newfield<ap_uint<8>,   boost::mpl::string<'hwle','n'> > hwlen;
    typedef newfield<ap_uint<8>,   boost::mpl::string<'plen'> > plen;
    typedef newfield<ap_uint<16>,  boost::mpl::string<'op'> > op;
    typedef newfield<ap_uint<8*6>, boost::mpl::string<'hwsr','c'> > hwsrc;
    typedef newfield<ap_uint<8*4>, boost::mpl::string<'psrc'> > psrc;
    typedef newfield<ap_uint<8*6>, boost::mpl::string<'hwds','t'> > hwdst;
    typedef newfield<ap_uint<8*4>, boost::mpl::string<'pdst'> > pdst;
    using header = fixed_header<boost::mpl::vector<hwtype,ptype,hwlen,plen,op,hwsrc,psrc,hwdst,pdst> >;
    template <typename T> header::parsed_hdr<T> parse_arp_hdr(T &h) { return header::parsed_hdr<T>(h); }
}

template <typename PayloadT>
class ipv4_hdr : public header<PayloadT, 20> {
public:
    struct ipv4_protocol {
        static const char ICMP = 0x01;
        static const char IGMP = 0x02;
        static const char TCP = 0x06;
        static const char UDP = 0x11;
    };
    typedef header<PayloadT, 20> HeaderT;
    // ipv4_hdr():HeaderT(),
    //                             version(*this, 0x45),
    //                             diffserv(*this, 0x0),
    //                             length(*this),
    //                             fragment_identifier(*this, 0),
    //                             fragment_offset(*this, 0),
    //                             TTL(*this, 0x40),
    //                             protocol(*this, ipv4_protocol::UDP),
    //                             checksum(*this, 0),
    //                             source(*this),
    //                             destination(*this) { }
    ipv4_hdr(PayloadT &payload):HeaderT(payload),
                                version(*this, 0x45),
                                diffserv(*this, 0x0),
                                lengths(*this),
                                fragment_identifier(*this, 0),
                                fragment_offset(*this, 0),
                                TTL(*this, 0x40),
                                protocol(*this, ipv4_protocol::UDP),
                                checksum(*this, 0),
                                source(*this),
                                destination(*this) { }

    using HeaderT::get_payload;
    using HeaderT::get;
    using HeaderT::set;

    typedef field<ap_uint<8>, HeaderT, HeaderT> versionT;
    typedef field<ap_uint<8>, versionT, HeaderT> diffservT;
    typedef field<ap_uint<16>, diffservT, HeaderT> lengthT;
    typedef field<ap_uint<16>, lengthT, HeaderT> fragment_identifierT;
    typedef field<ap_uint<16>, fragment_identifierT, HeaderT> fragment_offsetT;
    typedef field<ap_uint<8>, fragment_offsetT, HeaderT> TTLT;
    typedef field<ap_uint<8>, TTLT, HeaderT> protocolT;
    typedef field<ap_uint<16>, protocolT, HeaderT> checksumT;
    typedef field<ap_uint<32>, checksumT, HeaderT> sourceT;
    typedef field<ap_uint<32>, sourceT, HeaderT> destinationT;

    versionT version;
    diffservT diffserv;
    lengthT lengths;
    fragment_identifierT fragment_identifier;
    fragment_offsetT fragment_offset;
    TTLT TTL;
    protocolT protocol;
    checksumT checksum;
    sourceT source;
    destinationT destination;

    ap_uint<16> compute_ip_checksum() {
#pragma HLS inline 
        IPChecksum<32> checksum(0);
        ap_uint<4> ipHeaderLen = HeaderT::template get<1>(0).range(3, 0);
        // FIXME: Is there a natural width to do this checksum computation at?
    ComputeHeaderChecksum:
        for(int i = 0; i < ipHeaderLen; i++) {
            #pragma HLS pipeline II=1
            ap_uint<32> word = HeaderT::template get<4>(i*4);
            checksum.add_data_network_byte_order(byteSwap32(word));
        }
        return checksum.get();  // IP header checksum
    }

    void swapaddresses() {
        ap_uint<32> t = source.get();
        source.set(destination.get());
        destination.set(t);
    }
   void dump(std::ostream &out) {
#ifdef DEBUG
       out << "IPV4  : ";
        HeaderT::dump(out);
#endif
   }
};

namespace ipv4 {
    struct ipv4_protocol {
        static const char ICMP = 0x01;
        static const char IGMP = 0x02;
        static const char TCP = 0x06;
        static const char UDP = 0x11;
    };

    typedef newfield<ap_uint<8>,  boost::mpl::string<'vers'> > version;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'diff'> > diffserv;
    typedef newfield<ap_uint<16>, boost::mpl::string<'leng'> > length;
    typedef newfield<ap_uint<16>, boost::mpl::string<'frid'> > fragment_identifier;
    typedef newfield<ap_uint<16>, boost::mpl::string<'frof'> > fragment_offset;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'TTL' > > TTL;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'prot'> > protocol;
    typedef newfield<ap_uint<16>, boost::mpl::string<'csum'> > checksum;
    typedef newfield<ap_uint<32>, boost::mpl::string<'src' > > source;
    typedef newfield<ap_uint<32>, boost::mpl::string<'dest'> > destination;
    typedef newfield<ap_uint<16>, boost::mpl::string<'sprt'> > sport;
    typedef newfield<ap_uint<16>, boost::mpl::string<'dprt'> > dport;

    using header = fixed_header<boost::mpl::vector<version, diffserv, length, fragment_identifier, fragment_offset, TTL, protocol, checksum, source, destination> >;
    template <typename T> header::parsed_hdr<T> parse_ipv4_hdr(T &h) { return header::parsed_hdr<T>(h); }

    using udp_header = fixed_header<boost::mpl::vector<sport, dport, length, checksum> >;
    template <typename T> udp_header::parsed_hdr<T> parse_udp_hdr(T &h) { return udp_header::parsed_hdr<T>(h); }

    template <typename T>
    static ap_uint<16> compute_ip_checksum(T h) {
#pragma HLS inline
        IPChecksum<32> checksum(0);
        ap_uint<4> ipHeaderLen = h.template get<version>().range(3, 0);
        // FIXME: Is there a natural width to do this checksum computation at?
    ComputeHeaderChecksum:
        for(int i = 0; i < ipHeaderLen; i++) {
#pragma HLS pipeline II=1
            ap_uint<32> word = h.template get_le<4>(i*4);
            checksum.add_data_network_byte_order(word);
        }
        return checksum.get();  // IP header checksum
    }
};

namespace ipv6 {

    typedef newfield<ap_uint<4>,  boost::mpl::string<'vers'> > version;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'tcls'> > traffic_class;
    typedef newfield<ap_uint<20>, boost::mpl::string<'flow'> > flow_label;
    typedef newfield<ap_uint<16>, boost::mpl::string<'leng'> > length;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'next'> > next_header;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'hopl'> > hop_limit;
    typedef newfield<ap_uint<128>,boost::mpl::string<'src' > > source;
    typedef newfield<ap_uint<128>,boost::mpl::string<'dest'> > destination;

    using header = fixed_header<boost::mpl::vector<version, traffic_class, flow_label, length, next_header, hop_limit, source, destination> >;
    template <typename T> header::parsed_hdr<T> parse_ipv6_hdr(T &h) { return header::parsed_hdr<T>(h); }
};

template <typename PayloadT>
class icmp_hdr : public header<PayloadT, 8> {
public:
    typedef header<PayloadT, 8> HeaderT;
    // icmp_hdr():HeaderT(),
    //                             type(*this),
    //                             code(*this),
    //                             checksum(*this),
    //                             identifier(*this),
    //                             sequence_number(*this) { }
    icmp_hdr(PayloadT &payload):HeaderT(payload),
                                type(*this),
                                code(*this),
                                checksum(*this),
                                identifier(*this),
                                sequence_number(*this) { }

    using HeaderT::get_payload;
    using HeaderT::get;
    using HeaderT::set;

    typedef field<ap_uint<8>, HeaderT, HeaderT> typeT;
    typedef field<ap_uint<8>, typeT, HeaderT> codeT;
    typedef field<ap_uint<16>, codeT, HeaderT> checksumT;
    typedef field<ap_uint<16>, checksumT, HeaderT> identifierT;
    typedef field<ap_uint<16>, identifierT, HeaderT> sequence_numberT;

    typeT type;
    codeT code;
    checksumT checksum;
    identifierT identifier;
    sequence_numberT sequence_number;

    void dump(std::ostream &out) {
#ifdef DEBUG
        out << "ICMP  : ";
        HeaderT::dump(out);
#endif
    }
};
namespace icmp {
    typedef newfield<ap_uint<8>,  boost::mpl::string<'type'> > type;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'code'> > code;
    typedef newfield<ap_uint<16>, boost::mpl::string<'csum'> > checksum;
    typedef newfield<ap_uint<16>, boost::mpl::string<'iden'> > identifier;
    typedef newfield<ap_uint<16>, boost::mpl::string<'sqno'> > sequence_number;

    using header = fixed_header<boost::mpl::vector<type, code, checksum, identifier, sequence_number> >;
    template <typename T> header::parsed_hdr<T> parse_icmp_hdr(T &h) { return header::parsed_hdr<T>(h); }
};

template <typename PayloadT>
class udp_hdr : public header<PayloadT, 8> {
public:
    typedef header<PayloadT, 8> HeaderT;
    // udp_hdr():HeaderT(),
    //                            sport(*this),
    //                            dport(*this),
    //                            lengths(*this),
    //                            checksum(*this) { }
    udp_hdr(PayloadT &payload):HeaderT(payload),
                               sport(*this),
                               dport(*this),
                               lengths(*this),
                               checksum(*this, 0) { }

    using HeaderT::get_payload;
    using HeaderT::get;
    using HeaderT::set;

    // FIXME: handle variable length length field.
    typedef field<ap_uint<16>, HeaderT, HeaderT> sportT;
    typedef field<ap_uint<16>, sportT, HeaderT> dportT;
    typedef field<ap_uint<16>, dportT, HeaderT> lengthT;
    typedef field<ap_uint<16>, lengthT, HeaderT> checksumT;

    sportT sport;
    dportT dport;
    lengthT lengths;
    checksumT checksum;
};

namespace igmp {
    struct igmp_type {
        static const char membership_query = 0x11;
        static const char membership_report_v1 = 0x12;
        static const char membership_report_v2 = 0x16;
        static const char leave_group_v2 = 0x17;
        static const char membership_report_v3 = 0x22;
    };
    typedef newfield<ap_uint<8>, boost::mpl::string<'type'> > type;
    typedef newfield<ap_uint<8>, boost::mpl::string<'time'> > maxResponseTime;
    typedef newfield<ap_uint<16>, boost::mpl::string<'csum'> > checksum;
    typedef newfield<ap_uint<32>, boost::mpl::string<'gadd'> > groupAddress;
    typedef newfield<ap_uint<8>, boost::mpl::string<'type'> > flags;
    typedef newfield<ap_uint<8>, boost::mpl::string<'qqic'> > qqic;
    typedef newfield<ap_uint<16>, boost::mpl::string<'nums'> > numberOfFields;
    typedef newfield<ap_uint<16>, boost::mpl::string<'rsvd'> > reserved;
    typedef newfield<ap_uint<8>, boost::mpl::string<'alen'> > auxiliaryDataLength;

    using header = fixed_header<boost::mpl::vector<type, maxResponseTime, checksum> >;
    using membership_query_header = fixed_header<boost::mpl::vector<groupAddress, flags, qqic, numberOfFields> >;
    using membership_report_v3_header = fixed_header<boost::mpl::vector<reserved, numberOfFields> >;
    using group_record_header = fixed_header<boost::mpl::vector<type, auxiliaryDataLength, numberOfFields, groupAddress> >;

    template <typename PayloadT> using igmp_hdr                      = header::compound_hdr<PayloadT>;
    template <typename PayloadT> using igmp_membership_query_hdr     = membership_query_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using igmp_membership_report_v3_hdr = membership_report_v3_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using igmp_group_record_hdr         = group_record_header::compound_hdr<PayloadT>;

    template <typename T> header::parsed_hdr                       <T> parse_igmp_hdr                      (T &h) { return header::parsed_hdr<T>(h); }
    template <typename T> membership_query_header::parsed_hdr      <T> parse_igmp_membership_query_hdr     (T &h) { return membership_query_header::parsed_hdr<T>(h); }
    template <typename T> membership_report_v3_header::parsed_hdr  <T> parse_igmp_membership_report_v3_hdr (T &h) { return membership_report_v3_header::parsed_hdr<T>(h); }
    template <typename T> group_record_header::parsed_hdr          <T> parse_igmp_group_record_hdr         (T &h) { return group_record_header::parsed_hdr<T>(h); }
};

namespace mqttsn {
    struct mqttsn_type {
        static const unsigned char ADVERTISE = 0x00;
        static const unsigned char SEARCHGW = 0x01;
        static const unsigned char GWINFO = 0x02;
        static const unsigned char CONNECT = 0x04;
        static const unsigned char CONNACK = 0x05;
        static const unsigned char WILLTOPICREQ = 0x06;
        static const unsigned char WILLTOPIC = 0x07;
        static const unsigned char WILLMSGREQ = 0x08;
        static const unsigned char WILLMSG = 0x09;
        static const unsigned char REGISTER = 0x0A;
        static const unsigned char REGACK = 0x0B;
        static const unsigned char PUBLISH = 0x0C;
        static const unsigned char PUBACK = 0x0D;
        static const unsigned char PUBCOMP = 0x0E;
        static const unsigned char PUBREQ = 0x0F;
        static const unsigned char PUBREL = 0x10;
        static const unsigned char SUBSCRIBE = 0x12;
        static const unsigned char SUBACK = 0x13;
        static const unsigned char UNSUBSCRIBE = 0x14;
        static const unsigned char UNSUBACK = 0x15;
        static const unsigned char PINGREQ = 0x16;
        static const unsigned char PINGRESP = 0x17;
        static const unsigned char DISCONNECT = 0x18;
        static const unsigned char WILLTOPICUPD = 0x1A;
        static const unsigned char WILLTOPICRESP = 0x1B;
        static const unsigned char WILLMSGUPD = 0x1C;
        static const unsigned char WILLMSGRESP = 0x1D;

    };

    typedef newfield<ap_uint<16>, boost::mpl::string<'topi','cID'      > > topicID;
    typedef newfield<ap_uint<16>, boost::mpl::string<'mess','ageI','D' > > messageID;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'retu','rnCo','de'> > returnCode;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'leng','th'       > > length;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'type'            > > type;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'flag','s'        > > flags;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'prot','ocol'     > > protocol;
    typedef newfield<ap_uint<16>, boost::mpl::string<'dura','tion'     > > duration;

    using header           = fixed_header<boost::mpl::vector<length, type> >;
    using publish_header   = fixed_header<boost::mpl::vector<flags, topicID, messageID> >;
    using puback_header    = fixed_header<boost::mpl::vector<topicID, messageID, returnCode> >;
    using subscribe_header = fixed_header<boost::mpl::vector<flags, messageID> >;
    using suback_header    = fixed_header<boost::mpl::vector<flags, topicID, messageID, returnCode> >;
    using connect_header   = fixed_header<boost::mpl::vector<flags, protocol, duration> >;
    using connack_header   = fixed_header<boost::mpl::vector<returnCode> >;
    using register_header  = fixed_header<boost::mpl::vector<topicID, messageID> >;
    using regack_header    = fixed_header<boost::mpl::vector<topicID, messageID, returnCode> >;

    template <typename PayloadT> using mqttsn_hdr           = header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_publish_hdr   = publish_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_puback_hdr    = puback_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_subscribe_hdr = subscribe_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_suback_hdr    = suback_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_connect_hdr   = connect_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_connack_hdr   = connack_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_register_hdr  = register_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using mqttsn_regack_hdr    = regack_header::compound_hdr<PayloadT>;

    template <typename T> header::parsed_hdr           <T> parse_mqttsn_hdr           (T &h) { return header::parsed_hdr           <T>(h); }
    template <typename T> publish_header::parsed_hdr   <T> parse_mqttsn_publish_hdr   (T &h) { return publish_header::parsed_hdr   <T>(h); }
    template <typename T> puback_header::parsed_hdr    <T> parse_mqttsn_puback_hdr    (T &h) { return puback_header::parsed_hdr    <T>(h); }
    template <typename T> subscribe_header::parsed_hdr <T> parse_mqttsn_subscribe_hdr (T &h) { return subscribe_header::parsed_hdr <T>(h); }
    template <typename T> suback_header::parsed_hdr    <T> parse_mqttsn_suback_hdr    (T &h) { return suback_header::parsed_hdr    <T>(h); }
    template <typename T> connect_header::parsed_hdr   <T> parse_mqttsn_connect_hdr   (T &h) { return connect_header::parsed_hdr   <T>(h); }
    template <typename T> connack_header::parsed_hdr   <T> parse_mqttsn_connack_hdr   (T &h) { return connack_header::parsed_hdr   <T>(h); }
    template <typename T> register_header::parsed_hdr  <T> parse_mqttsn_register_hdr  (T &h) { return register_header::parsed_hdr  <T>(h); }
    template <typename T> regack_header::parsed_hdr    <T> parse_mqttsn_regack_hdr    (T &h) { return regack_header::parsed_hdr    <T>(h); }
}

namespace mold64 {
    static const int END_OF_SESSION = 0xFFFF;
    typedef newfield<ap_uint<80>, boost::mpl::string<'sess'> > session;
    typedef newfield<ap_uint<64>, boost::mpl::string<'seq'> > sequenceNumber;
    typedef newfield<ap_uint<16>, boost::mpl::string<'cnt'> > messageCount;
    typedef newfield<ap_uint<16>, boost::mpl::string<'leng'> > messageLength;

    using header           = fixed_header<boost::mpl::vector<session, sequenceNumber, messageCount> >;
    using message_header   = fixed_header<boost::mpl::vector<messageLength> >;

    template <typename PayloadT> using mold_hdr           = header::compound_hdr<boost::mpl::vector<session, sequenceNumber, messageCount> >;
    template <typename PayloadT> using mold_message_hdr   = message_header::compound_hdr<boost::mpl::vector<messageLength> >;

    template <typename T> header::parsed_hdr             <T> parse_mold_hdr             (T &h) { return header::parsed_hdr<T>(h); }
    template <typename T> message_header::parsed_hdr     <T> parse_mold_message_hdr     (T &h) { return message_header::parsed_hdr   <T>(h); }
}
namespace itch {
    typedef ap_uint<16> OrderReference;
    // common header
    typedef newfield<ap_uint<8>,  boost::mpl::string<'type'> > messageType; // in {S,R,H
    typedef newfield<ap_uint<16>, boost::mpl::string<'sloc'> > stockLocate;
    typedef newfield<ap_uint<16>, boost::mpl::string<'trck'> > trackingNumber;
    typedef newfield<ap_uint<48>, boost::mpl::string<'time'> > timestamp;

    //4.1
    typedef newfield<ap_uint<8>,  boost::mpl::string<'even'> > event; // in {O,S,Q,M,E,C}
    //4.2.1
    typedef newfield<ap_uint<64>, boost::mpl::string<'stck'> > stock;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'mcat'> > marketCategory; // in {Q,G,S,N,A,P,Z,V,' '}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'stat'> > financialStatus; // in {D,E,Q,S,G,H,J,K,C,N,' '}
    typedef newfield<ap_uint<32>, boost::mpl::string<'rlsz'> > roundLotSize;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'rlon'> > roundLotsOnly; // in {Y,N}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'clas'> > issueClassification;
    typedef newfield<ap_uint<16>, boost::mpl::string<'subt'> > issueSubtype;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'auth'> > authenticity; // in {P,T}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'shrt'> > shortSale; // in {Y,N,' '}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'ipof'> > IPOFlag; // in {Y,N,' '}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'tier'> > LULDReferenceTier; // in {1,2,' '}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'efla'> > ETPFlag; // in {Y,N,' '}
    typedef newfield<ap_uint<32>, boost::mpl::string<'efac'> > ETPFactor; 
    typedef newfield<ap_uint<8>,  boost::mpl::string<'invi'> > inverseIndicator; // in {Y,N}

    //4.2.2
    typedef newfield<ap_uint<8>,  boost::mpl::string<'stat'> > tradingState; // in {H,P,Q,T
    typedef newfield<ap_uint<8>,  boost::mpl::string<'rsvd'> > reserved;
    typedef newfield<ap_uint<32>, boost::mpl::string<'stat'> > reason;

    //4.2.3
    typedef newfield<ap_uint<8>,  boost::mpl::string<'shoa'> > regSHOAction; // in {0,1,2}
    //4.2.4
    typedef newfield<ap_uint<32>,  boost::mpl::string<'mpid'> > MPID;
    typedef newfield<ap_uint<8>,  boost::mpl::string<'mkrf'> > primaryMarketMaker; // in {Y,N}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'mkrm'> > marketMakerMode; // in {N,P,S,R,L}
    typedef newfield<ap_uint<8>,  boost::mpl::string<'mkrs'> > marketParticipantState; // in {A,E,W,S,D}

    //4.2.5
    typedef newfield<ap_uint<64>,  boost::mpl::string<'lvl1'> > level1;
    typedef newfield<ap_uint<64>,  boost::mpl::string<'lvl2'> > level2;
    typedef newfield<ap_uint<64>,  boost::mpl::string<'lvl3'> > level3;
    typedef newfield<ap_uint<8>,   boost::mpl::string<'blvl'> > breachedLevel;

    //4.2.6
    //4.2.7
    
    //4.3, 4.4
    typedef newfield<OrderReference,  boost::mpl::string<'ordr'> > orderReference;
    typedef newfield<ap_uint<64>,  boost::mpl::string<'bysl'> > buySell; // in {B,S}
    typedef newfield<ap_uint<32>,  boost::mpl::string<'shar'> > shares;
    typedef newfield<ap_uint<32>,  boost::mpl::string<'pric'> > price;
    // Stock
    typedef newfield<ap_uint<32>,  boost::mpl::string<'attr'> > attribution;
    typedef newfield<ap_uint<32>,  boost::mpl::string<'mano'> > matchNumber;
    typedef newfield<ap_uint<8>,   boost::mpl::string<'blvl'> > printable;
    typedef newfield<OrderReference,  boost::mpl::string<'oord'> > originalOrderReference;
    typedef newfield<OrderReference,  boost::mpl::string<'nord'> > newOrderReference;

    using header                       = fixed_header<boost::mpl::vector<messageType, stockLocate, trackingNumber, timestamp> >;
    using directory_header             = fixed_header<boost::mpl::vector<stock, marketCategory, financialStatus, roundLotSize, roundLotsOnly, issueClassification, issueSubtype, authenticity, shortSale, IPOFlag, LULDReferenceTier, ETPFlag, ETPFactor, inverseIndicator> >; // type 'R'
    using trading_action_header        = fixed_header<boost::mpl::vector<stock, tradingState, reserved, reason> >; // type 'H'
    using add_order_header             = fixed_header<boost::mpl::vector<orderReference, buySell, shares, stock, price> >; // type 'A'
    using add_order_attribution_header = fixed_header<boost::mpl::vector<orderReference, buySell, shares, stock, price, attribution> >; // type 'F'
    using order_executed_header        = fixed_header<boost::mpl::vector<orderReference, shares, matchNumber> >; // type 'E'
    using order_executed_price_header  = fixed_header<boost::mpl::vector<orderReference, shares, matchNumber, printable, price> >; // type 'C'
    using order_cancel_header          = fixed_header<boost::mpl::vector<orderReference, shares, matchNumber> >; // type 'X'
    using order_delete_header          = fixed_header<boost::mpl::vector<orderReference> >; // type 'D'
    using order_replace_header         = fixed_header<boost::mpl::vector<originalOrderReference, newOrderReference, shares, price> >; // type 'U'


    template <typename PayloadT> using itch_hdr                       = header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_directory_hdr             = directory_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_trading_action_hdr        = trading_action_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_add_order_hdr             = add_order_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_add_order_attribution_hdr = add_order_attribution_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_order_executed_hdr        = order_executed_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_order_executed_price_hdr  = order_executed_price_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_order_cancel_hdr          = order_cancel_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_order_delete_hdr          = order_delete_header::compound_hdr<PayloadT>;
    template <typename PayloadT> using itch_order_replace_hdr         = order_replace_header::compound_hdr<PayloadT>;


    template <typename T> header::parsed_hdr                         <T> parse_itch_hdr                         (T &h) { return header::parsed_hdr                       <T>(h); }
    template <typename T> directory_header::parsed_hdr               <T> parse_itch_directory_hdr               (T &h) { return directory_header::parsed_hdr             <T>(h); }
    template <typename T> trading_action_header::parsed_hdr          <T> parse_itch_trading_action_hdr          (T &h) { return trading_action_header::parsed_hdr        <T>(h); }
    template <typename T> add_order_header::parsed_hdr               <T> parse_itch_add_order_hdr               (T &h) { return add_order_header::parsed_hdr             <T>(h); }
    template <typename T> add_order_attribution_header::parsed_hdr   <T> parse_itch_add_order_attribution_hdr   (T &h) { return add_order_attribution_header::parsed_hdr <T>(h); }
    template <typename T> order_executed_header::parsed_hdr          <T> parse_itch_order_executed_hdr          (T &h) { return order_executed_header::parsed_hdr        <T>(h); }
    template <typename T> order_executed_price_header::parsed_hdr    <T> parse_itch_order_executed_price_hdr    (T &h) { return order_executed_price_header::parsed_hdr  <T>(h); }
    template <typename T> order_cancel_header::parsed_hdr            <T> parse_itch_order_cancel_hdr            (T &h) { return order_cancel_header::parsed_hdr             <T>(h); }
    template <typename T> order_delete_header::parsed_hdr            <T> parse_itch_order_delete_hdr            (T &h) { return order_delete_header::parsed_hdr          <T>(h); }
    template <typename T> order_replace_header::parsed_hdr           <T> parse_itch_order_replace_hdr           (T &h) { return order_replace_header::parsed_hdr         <T>(h); }

}

template<typename _DATA_T>
class ArrayReader
{
public:
    typedef _DATA_T DATA_T;
protected:
    const int length;
    DATA_T *data;
    int index;
public:
    ArrayReader(DATA_T *_data, int _length):
        length(_length), data(_data), index(0)
    {
#pragma HLS inline
    }

    DATA_T get() {
#pragma HLS inline
        return data[index++];
    }
    bool empty() {
#pragma HLS inline
        return index < length;
    }
};
template<typename _DATA_T>
class StreamReader
{
public:
    typedef _DATA_T DATA_T;
protected:
    hls::stream<_DATA_T> &stream;
public:
    StreamReader(hls::stream<_DATA_T> &_stream):
        stream(_stream)
    {
#pragma HLS inline
    }

    DATA_T get() {
#pragma HLS inline
        return stream.read();
    }

    bool empty() {
#pragma HLS inline
        return stream.empty();
    }
};

// Flag the last kept byte.
// 1111 -> 1000
// 0111 -> 0100
// 0011 -> 0010
// 0001 -> 0001
template<int N>
ap_uint<N> lastflag(ap_uint<N> keep, bool last) {
#pragma HLS inline
#ifndef __SYNTHESIS__
    for(int i = 1; i < N; i++) {
        if(keep[i] && !keep[i-1]) {
            std::cout << "Sparse keep not allowed: " << std::hex << keep << "\n."; assert(false);
        }
    }
#endif
    ap_uint<N> flag =  keep ^ (last ? (keep >> 1) : keep);
    return flag;
}
template<typename T, int N, int scale>
struct Shifter {
    static T rshiftbytes(T t, ap_uint<N> s) {
        ap_uint<N-1> s2 = s>>1;
        T t2 = Shifter<T,N-1,scale*2>::rshiftbytes(t, s2);
        if(s[0]) return t2 >> (scale*8); else return t2;
    }
};
template<typename T, int scale>
struct Shifter<T, 1, scale> {
    static T rshiftbytes (T t, ap_uint<1> s) {
        if(s[0]) return t >> (scale*8); else return t;
    }
};
template<typename T, int N>
T rshiftbytes(T t, ap_uint<N> s) {
    T x = Shifter<T,N,1>::rshiftbytes(t,s);
    assert(x == (t >> s*8));
    return x;
}
template<typename READER_T>
class LittleEndianByteReader
{
    typedef typename READER_T::DATA_T DATA_T;
    const static int DATA_N = width_traits<DATA_T>::WIDTH; //DATA_T::WIDTH;//Type_BitWidth<DATA_T>::Value;
    const static int DATA_S = DATA_N/8;
    typedef ap_uint<DATA_N> BUFFER_T;
    typedef ap_uint<DATA_S> KEEP_T;
public:
	LittleEndianByteReader(READER_T input)
		: m_input(input), m_buffer(0), m_keep(0), m_last(0), m_bytesBuffered(0) {
#pragma HLS inline
    }
	unsigned int BytesBuffered() const {return m_bytesBuffered;}

    // Return the next T from the input stream.
    template<typename T>
    void get(T &t) {
#pragma HLS inline
        const int S = T::LENGTH;//N/8;
        const int N = S*8;//Type_BitWidth<T>::Value;

        const int BUFN = N+8*DATA_S*2;
        const int BUFS = S+DATA_S*2;
        //ap_uint<BitWidth<length>::Value>
        int t_bytesBuffered = m_bytesBuffered;
        ap_uint<BUFN> t_buffer = m_buffer;
        ap_uint<BUFS> t_keep = m_keep;
        ap_uint<BUFS> t_last = m_last;

        // Basically there are two cases below: one where we read data and
        // one where we don't.   In either case we need to shift the buffer
        // correctly at the end.  If we read data, then the new buffer only
        // consists of newly read data.

        BUFFER_T data;
        KEEP_T keep;
        KEEP_T t_lastflag;
    get_loop:
        for(int i = 0; i < (S+DATA_S-1)/DATA_S; i++)
#pragma HLS unroll
            if (t_bytesBuffered < S) {
                DATA_T b;
                assert(!m_input.empty());
                b = m_input.get();
                data = b.data;
                keep = b.keep;
                t_lastflag = lastflag(b.keep, b.last);
                ap_uint<BUFN> shifted = ap_uint<BUFN>(data) << (i+1)*DATA_S*8; // variable shift
                t_buffer |= shifted;
                t_keep |= ap_uint<BUFS>(keep) << (i+1)*DATA_S;
                t_last |= ap_uint<BUFS>(t_lastflag) << (i+1)*DATA_S;
                t_bytesBuffered += DATA_S;
                //std::cout << t_bytesBuffered << " " << t_buffer.to_string(16) << " " << t_keep.to_string(2) << " " << t_lastflag.to_string(2) << "\n";
            }

        ap_uint<BitWidth<DATA_S>::Value> lastshift = DATA_S-m_bytesBuffered;
        // ap_uint<N> result = t_buffer >> lastshift*8;
        ap_uint<N> result = rshiftbytes(t_buffer, lastshift);
        t.template set_le<S>(0, result);
        //std::cout << result.to_string(16) << " = get(" << S << "): ";

        // We are going to read K=ceil((S-m_bytesBuffered)/DATA_S) beats
        // And have (DATA_S*K)-(S-m_bytesBuffered) bytes remaining
        // or m_bytesBuffered-S%DATA_S mod DATA_S
        m_bytesBuffered -= S%DATA_S;

        m_buffer = data;
        //std::cout << keep.to_string(2) << " " << keepFlag<DATA_S>(m_bytesBuffered) << "\n";
        m_keep = keep & keepFlag<DATA_S>(m_bytesBuffered);
        m_last = t_lastflag;
        //std::cout << m_bytesBuffered << " " << (m_buffer >> (DATA_S-m_bytesBuffered)*8).to_string(16) << " " << m_keep.to_string(2) << " " << m_last.to_string(2) << "\n";
    }

    template<typename T>
    T get() {
#pragma HLS inline
        T t;
        get(t);
        return t;
    }

    // Return the next databeat from the input stream.
    void read(DATA_T &t) {
#pragma HLS inline
        DATA_T b;
        b.data = 0;
        b.keep = 0;
        b.last = 0;
        if(m_last == 0) {
            assert(!m_input.empty());
            b = m_input.get();
        }
        const int BUFN = 8*DATA_S*2;
        const int BUFS = DATA_S*2;
        //std::cout << "read " << b.data.to_string(16) << " " << b.keep.to_string(2) << " " << b.last.to_string(2) << "\n";
        ap_uint<BUFN> data = b.data;
        ap_uint<BUFS> keep = b.keep;
        ap_uint<BUFS> t_lastflag = lastflag(b.keep, b.last);
        ap_uint<BUFN> t_buffer = m_buffer;
        ap_uint<BUFS> t_keep = m_keep;
        ap_uint<BUFS> t_last = m_last;
        t_buffer(BUFN-1, DATA_S*8) = data;
        t_keep(BUFS-1, DATA_S) = keep;
        t_last(BUFS-1, DATA_S) = t_lastflag;
        //std::cout << "read("<<DATA_S<< "): " << m_bytesBuffered << " " << t_buffer.to_string(16) << " " << t_keep.to_string(2) << " " << t_last.to_string(2) << "\n";
        t.data = t_buffer >> (DATA_S-m_bytesBuffered)*8;
        t.keep = t_keep >> DATA_S-m_bytesBuffered;
        t.last = (t_last >> DATA_S-m_bytesBuffered)!= 0;
        //std::cout << "read("<<DATA_S<< "): " << t.data.to_string(16) << " " << t.keep.to_string(2) << " " << t.last.to_string(2) << "\n";
        m_buffer = data;
        m_keep = keep;
        m_last = t_lastflag;
        //std::cout << "read("<<DATA_S<< "): " << m_buffer.to_string(16) << " " << m_keep.to_string(2) << " " << m_last.to_string(2) << "\n";
    }

    DATA_T read() {
#pragma HLS inline
        DATA_T t;
        read(t);
        return t;
    }

    // Return remaining inputs until TLAST is reached.
    void read_rest() {
#pragma HLS inline
        const int BUFN = 8*DATA_S*2;
        const int BUFS = DATA_S*2;
        ap_uint<BUFN> t_buffer = m_buffer;
        ap_uint<BUFS> t_keep = m_keep;
        ap_uint<BUFS> t_last = m_last;
        bool done_reading = m_last != 0;

        // std::cout << "readrest("<<DATA_S<< "): " << t_buffer.to_string(16) << " " << t_keep.to_string(2) << " " << t_last.to_string(2) << "\n";
    read_rest_loop:
        while (!done_reading) {
#pragma HLS pipeline
            DATA_T in;
            in.data = 0;
            in.keep = 0;
            in.last = 0;
            // std::cout << "read("<<DATA_S<< "): " << in.data.to_string(16) << " " << in.keep.to_string(2) << " " << in.last.to_string(2) << "\n";
            if(!done_reading) {
                assert(!m_input.empty());
                in = m_input.get();
            }
            ap_uint<BUFS> t_lastflag = lastflag(in.keep, in.last);
            t_buffer |= ap_uint<BUFN>(in.data) << DATA_S*8;
            t_keep |= ap_uint<BUFS>(in.keep) << DATA_S;
            t_last |= ap_uint<BUFS>(t_lastflag) << DATA_S;
            // std::cout << "readrest("<<DATA_S<< "): " << t_buffer.to_string(16) << " " << t_keep.to_string(2) << " " << t_last.to_string(2) << "\n";
            done_reading = in.last;
        }
        m_bytesBuffered = 0;
    }

    // do we need this?
    bool empty() {
#pragma HLS inline
        ap_uint<DATA_S> keep = m_keep(DATA_S-1,0);
        return keep != -1;
    }

    // do we need this?
    void reset() {
        m_bytesBuffered = 0;
    }

private:
    READER_T m_input;
	BUFFER_T m_buffer;
    KEEP_T m_keep;
    KEEP_T m_last;
	ap_uint<UnsignedBitWidth<DATA_S>::Value> m_bytesBuffered;
};

template<typename _DATA_T>
class StreamWriter
{
public:
    typedef _DATA_T DATA_T;
protected:
    hls::stream<_DATA_T> &stream;
public:
    StreamWriter(hls::stream<_DATA_T> &_stream):
        stream(_stream)
    {
#pragma HLS inline
    }

    void put(DATA_T d) {
#pragma HLS inline
        return stream.write(d);
    }
};
template<typename WRITER_T>
class LittleEndianByteWriter
{
    typedef typename WRITER_T::DATA_T DATA_T;
    const static int DATA_N = width_traits<DATA_T>::WIDTH; //DATA_T::WIDTH;//Type_BitWidth<DATA_T>::Value;
    const static int DATA_S = DATA_N/8;
    typedef ap_uint<DATA_N> BUFFER_T;
    typedef ap_uint<DATA_S> KEEP_T;
public:
	LittleEndianByteWriter(WRITER_T output)
		: m_output(output), m_buffer(0), m_keep(0), m_last(0), m_bytesBuffered(0) {}
	unsigned int BytesBuffered() const {return m_bytesBuffered;}

    // Push the next T to the input stream.
    template<typename T>
    void put(T t) {
#pragma HLS inline
        const int S = T::LENGTH;
        const int N = S*8;//Type_BitWidth<T>::Value;
        int t_bytesBuffered = m_bytesBuffered;
        ap_uint<N+DATA_N> data = t.template get_le<S>(0);
        ap_uint<S+DATA_S> keep = ap_uint<S>(-1);
        ap_uint<S+DATA_S> last = 0;
        data <<= m_bytesBuffered*8;
        keep <<= m_bytesBuffered;
        last <<= m_bytesBuffered;

        if(m_bytesBuffered > 0) {
            data(m_bytesBuffered*8-1,0) = m_buffer(m_bytesBuffered*8-1,0);
            keep(m_bytesBuffered-1,0) = m_keep(m_bytesBuffered-1,0);
            last(m_bytesBuffered-1,0) = m_last(m_bytesBuffered-1,0);
        }

        t_bytesBuffered += S;
        m_buffer = data;
        m_keep = keep;
        m_last = last;
        m_bytesBuffered = t_bytesBuffered;
        for(int i = 0; i < (S+DATA_S-1)/DATA_S; i++)
        if (t_bytesBuffered >= DATA_S) {
#pragma HLS unroll
            ap_uint<N> x = data(N-1,0);
            ap_uint<S> keep_x = keep(S-1,0);

            DATA_T out;
            out.data = x;
            out.last = false;
            out.keep = -1;
            m_output.put(out);
            data >>= DATA_N;
            keep >>= DATA_S;
            last >>= DATA_S;
            t_bytesBuffered -= DATA_S;
            m_buffer = data;
            m_keep = keep;
            m_last = last;
            m_bytesBuffered = t_bytesBuffered;
            //std::cout << "rest: " << m_buffer.to_string(16) << " " << m_keep.to_string(2) << " " << m_last.to_string(2) <<  "\n";
            //std::cout << t_bytesBuffered << " remaining\n";
        }
        //std::cout << "put("<<S<< "," << S/DATA_S << "): " << m_bytesBuffered << " remaining\n";
    }

    // Copy remaining input data (until TLAST) from the given input stream to this output stream.
    // Output all buffered data in this writer, generating TLAST at the end of the frame.
    // The input stream must have the same width as this output stream.
    void put_rest(LittleEndianByteReader<StreamReader<DATA_T> > reader) {
#pragma HLS inline
        bool done = false;
        bool done_reading = false;
        ap_uint<DATA_N> t_buffer = m_buffer;
        ap_uint<DATA_S> t_keep = m_keep;
        ap_uint<DATA_S> t_last = m_last;
        while (!done) {
#pragma HLS pipeline
            DATA_T in;
            in.data = 0;
            in.keep = 0;
            in.last = 0;
            if(!done_reading)
                in = reader.read();
            ap_uint<DATA_S> t_lastflag = lastflag(in.keep, in.last);
            assert(t_buffer(DATA_N-1,m_bytesBuffered*8) == 0);
            assert(t_keep(DATA_S-1,m_bytesBuffered) == 0);
            t_buffer |= ap_uint<DATA_N>(in.data) << m_bytesBuffered*8;
            t_keep |= ap_uint<DATA_S>(in.keep) << m_bytesBuffered;
            t_last |= ap_uint<DATA_S>(t_lastflag) << m_bytesBuffered;
            done_reading = in.last;
            DATA_T out;
            out.data = t_buffer(DATA_N-1,0);
            out.keep = t_keep(DATA_S-1,0);
            out.last = t_last(DATA_S-1,0) != 0;
            //std::cout << "put_rest("<<DATA_S<< "): " << out.data.to_string(16) << " " << out.keep.to_string(2) << " " << out.last.to_string(2) << "\n";
            done = out.last;
            m_output.put(out);
            t_buffer = in.data >> (DATA_N-m_bytesBuffered*8);//m_buffer >>= DATA_N;
            t_keep = KEEP_T(in.keep) >> (DATA_S-m_bytesBuffered);
            t_last = KEEP_T(t_lastflag) >> (DATA_S-m_bytesBuffered);
            //std::cout << "rest: " << t_buffer.to_string(16) << " " << m_keep.to_string(2) << " " << m_last.to_string(2) <<  "\n";
        }
    }

    // Write the next databeat to the output stream.
    bool write(DATA_T b) {
#pragma HLS inline
        ap_uint<DATA_N> data = b.data;
        ap_uint<DATA_S> keep = b.keep;
        ap_uint<DATA_S> t_lastflag = lastflag(b.keep, b.last);
        ap_uint<DATA_N> t_buffer = m_buffer;
        ap_uint<DATA_S> t_keep = m_keep;
        ap_uint<DATA_S> t_last = m_last;
        t_buffer |= ap_uint<DATA_N>(data) << m_bytesBuffered*8;
        t_keep |= ap_uint<DATA_S>(keep) << m_bytesBuffered;
        t_last |= ap_uint<DATA_S>(t_lastflag) << m_bytesBuffered;

        DATA_T out;
        out.data = t_buffer;
        out.keep = t_keep;
        out.last = t_last != 0;

        m_output.put(out);
        m_buffer = data >> (DATA_N-m_bytesBuffered*8);
        m_keep = keep >> (DATA_S-m_bytesBuffered);
        m_last = t_lastflag >> (DATA_S-m_bytesBuffered);
        return out.last;
    }

    // Flush the last data beat, if necessary
    void flush() {
#pragma HLS inline
        DATA_T out;
        out.data = m_buffer;
        out.keep = m_keep;
        out.last = 1;
        if(m_bytesBuffered > 0) {
            assert(m_last != 0);
            m_output.put(out);
        }
        m_bytesBuffered = 0;
        m_keep = 0;
        m_last = 0;
    }

    // Do we need this?
    void reset() {
        m_bytesBuffered = 0;
    }

private:
    WRITER_T m_output;
	BUFFER_T m_buffer;
    KEEP_T m_keep;
    KEEP_T m_last;
	ap_uint<UnsignedBitWidth<DATA_S>::Value> m_bytesBuffered;
};

template<typename axiWord>
LittleEndianByteReader<StreamReader<axiWord> > make_reader(hls::stream<axiWord> &stream) {
    StreamReader<axiWord> r(stream);
    LittleEndianByteReader<StreamReader<axiWord> > reader(r);
    return reader;
}

template<typename axiWord>
LittleEndianByteWriter<StreamWriter<axiWord> > make_writer(hls::stream<axiWord> &stream) {
    StreamWriter<axiWord> r(stream);
    LittleEndianByteWriter<StreamWriter<axiWord> > writer(r);
    return writer;
}

template<typename PayloadT, int length>
static std::ostream & operator <<(std::ostream &stream, const header<PayloadT, length> &p) {
#ifndef __SYNTHESIS__
    stream << std::hex << std::noshowbase;
    stream << std::setfill('0');
    for(int i = 0; i < length; i++) {
        stream << std::setw(2) << (unsigned int)p.template get<1>(i);
    }
    stream << std::dec;
    stream << " + " << p.p.data_length() << " bytes";
#endif
    return stream;
}
template<int length>
static std::ostream & operator <<(std::ostream &stream, const generic_header<length> &p) {
#ifndef __SYNTHESIS__
    stream << std::hex << std::noshowbase;
    stream << std::setfill('0');
    for(int i = 0; i < length; i++) {
        stream << std::setw(2) << (unsigned int)p.template get<1>(i);
    }
    stream << std::dec;
#endif
    return stream;
}
template<typename BackT>
static std::ostream & operator <<(std::ostream &stream, const remainder_header<BackT> &p) {
    stream << "Packet with " << p.data_length() << " bytes";
    return stream;
}
template<typename BackT, typename _Fields>
static std::ostream & operator <<(std::ostream &stream, const parsed_header<BackT, _Fields> &p) {
    stream << std::hex << std::noshowbase;
    stream << std::setfill('0');
    for(int i = 0; i < p.LENGTH; i++) {
        stream << std::setw(2) << (unsigned int)p.template get<1>(i);
    }
    stream << std::dec;
    return stream;
}
static std::ostream & operator <<(std::ostream &stream, const Packet &p) {
    stream << "Packet with " << p.data_length() << " bytes";
    return stream;
}

template<int N, typename T>
void dumpAsCArray(T packet) {
    int len = packet.data_length();
    printf("len = %d;\n", len);
    printf("{\n");
    int count = 0;
    for (int i = 0; i < (len+N-1)/N; i++) {
        printf("/*%05x*/  ", i*N);
        for(int j = 0; j < N; j ++) {
            if(i*N+j >= len) {
                printf("   ");
            } else {
                unsigned char c = packet.template get<1>(count++);
                printf("0x%02x,", c);
            }
        }
        printf("\n");
    }
    printf("}\n");
}

#pragma GCC diagnostic pop
