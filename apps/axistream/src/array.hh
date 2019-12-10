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

/*
 * Simple array type for use in HLS.
 * This serves as a translatable version of std::array,
 * for cases when an array with value semantics is needed.
 * (As opposed to normal C arrays which decay to pointers.)
 */
template <typename T, int N>
struct array {
    T data[N];

    static int size() {
        HLS_PRAGMA(HLS inline);
        return N;
    }

    const T& operator[](int i) const {
        HLS_PRAGMA(HLS inline);
        return data[i];
    }

    T& operator[](int i) {
        HLS_PRAGMA(HLS inline);
        return data[i];
    }
};
