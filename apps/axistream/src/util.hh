#pragma once

#include "hls_int.hh"
#include "hls_macros.hh"

namespace util {

constexpr int BITS_PER_BYTE = 8;

template <int B>
using Bytes = ap_uint<B * BITS_PER_BYTE>;

template <int num>
struct clog2 {
    static_assert(num >= 2, "num >= 2");
    static const int value = clog2<(num + 1) / 2>::value + 1;
};

template <>
struct clog2<2> {
    static const int value = 1;
};

template <int num>
struct unsigned_bit_width {
    static const int value = clog2<num + 1>::value;
};

}
