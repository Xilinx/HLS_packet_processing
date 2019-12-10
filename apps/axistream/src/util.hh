/*
 *    Copyright 2017 Two Sigma Open Source, LLC
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#pragma once

#include "hls_macros.hh"
#include "hls_int.hh"

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
struct is_power_of_2 {
    static_assert(num >= 0, "num >= 0");
    static const bool value = is_power_of_2<num / 2>::value && (num % 2 == 0);
};

template <>
struct is_power_of_2<1> {
    static const bool value = true;
};

template <>
struct is_power_of_2<0> {
    static const bool value = false;
};

template <int num>
struct unsigned_bit_width {
    static const int value = clog2<num + 1>::value;
};

template <int num_states>
using state_t = ap_uint<clog2<num_states>::value>;

template <int max_value>
using uint_t = ap_uint<unsigned_bit_width<max_value>::value>;

template <int... X>
struct Sum;

template <int A, int... X>
struct Sum<A, X...> {
    static const int value = A + Sum<X...>::value;
};

template <>
struct Sum<>{
    static const int value = 0;
};

}
