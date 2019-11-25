#pragma once
#include "hls_macros.hh"

namespace util {

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

}
