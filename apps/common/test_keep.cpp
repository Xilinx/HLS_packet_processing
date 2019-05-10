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
#include <typeinfo>
#include <string>
#include <cxxabi.h>

template<int N>
int count(ap_uint<N> x) {
    int count = 0;
    for(int i = 0; i < N; i ++) {
        if(x[i]) count ++;
    }
    return count;
}

/*template<int N>
ap_uint<N> invert(ap_uint<N> x) {
    for(int i = 0; i < N; i ++) {
        x[i] = ~x[i];
    }
    return x;
    }*/
template<typename T>
std::string demangle(T x) {
    std::string s = typeid(x).name();
    int status = 0;
    char* demangled = abi::__cxa_demangle(s.c_str(), 0, 0, &status);
    return demangled;
}

/*template<int N>
int keptbytes(ap_uint<N> keep) {
#ifndef __SYNTHESIS__
    for(int i = 1; i < N; i++) {
        if(keep[i] && !keep[i-1]) {
            std::cout << "Sparse keep not allowed: " << std::hex << keep << "\n."; assert(false);
        }
    }
#endif
    // std::string s = typeid(~keep).name();
    // int status = 0;
    // char* demangled = abi::__cxa_demangle(s.c_str(), 0, 0, &status);

    // std::string s = typeid(invert(keep)).name();
    // int status = 0;
    // char* demangled2 = abi::__cxa_demangle(s.c_str(), 0, 0, &status);
    std::cout << "Current: " << std::hex
              << ~keep << " "
              << demangle(~keep) << " " 
              << ap_uint<N+1>(~keep) << " " << ap_uint<N+1>(~keep).reverse() << " "
              << ~ap_uint<N+1>(keep) << " " << ~ap_uint<N+1>(keep).reverse() << "\n";
    std::cout << "Correct: " << std::hex
              << invert(keep) << " "
              << demangle(invert(keep)) << " " 
              << ap_uint<N+1>(invert(keep)) << " " << ap_uint<N+1>(invert(keep)).reverse() << " "
              << invert(ap_uint<N+1>(keep)) << " " << invert(ap_uint<N+1>(keep)).reverse() << "\n";

    return ap_uint<N+1>(~keep).reverse().countLeadingZeros();
    }*/
template<typename T>
bool test(T x) {
    int c = count(x);
    int c2 = keptbytes(x);
    if(c != c2) std::cout << "Failed: " << std::hex << x << std::dec
                          << " " << c << " " << c2 << "\n";
    return c == c2;
}
int main() {
    test(ap_uint<32>(0xFFFFFFFF));
    test(ap_uint<16>(0xFFFF));
    test(ap_uint<32>(0xFFFF));
    test(ap_uint<64>(0xFFFFFFFFFFFFFFFF));
    test(ap_uint<32>(0xFFFFFFF));
    test(ap_uint<68>(-1));
}
