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

struct nullopt {};

/*
 * Represents an optional type, much like https://en.cppreference.com/w/cpp/utility/optional.
 * However, the std::optional won't synthesize, because it internally uses a reinterpret_cast --
 * instead of storing an instance of type T, it instead stores a byte buffer of the proper size,
 * and only constructs an instance of T within that buffer when required.
 * As such, reading the value requires a reinterpret_cast.
 *
 * To avoid casting, this class stores an instance of T. Since it does so, however,
 * T must be default constructable -- there is no way to defer construction.
 *
 * This class is useful for functions which may or may not return a result.
 * For example, a get function on a hash table might have the signature:
 * optional<T> get(const Key& key)
 * This function would either return the value corresponding to the given key,
 * if there is a mapping for the given key in the hash table,
 * or return an empty optional.
 * The caller of get could then inspect the optional with operator bool() to
 * determine if the mapping was found. For example:
 *
 * optional<int> x = ht.get(5);
 * if (bool(x)) {
 *     // 5 was found, extract the value from the optional with *
 *     std::cout << *x << std::endl;
 * } else {
 *     std::cout << "not found" << std::endl;
 * }
 */
template <typename T>
class optional {
public:
    optional() : _valid(false) {
        HLS_PRAGMA(HLS inline);
    }
    optional(const T& value) : _value(value), _valid(true) {
        HLS_PRAGMA(HLS inline);
    }
    optional(nullopt) : optional() {
        HLS_PRAGMA(HLS inline);
    }

    const T* operator->() const {
        HLS_PRAGMA(HLS inline);
        return &_value;
    }

    T* operator->() {
        HLS_PRAGMA(HLS inline);
        return &_value;
    }

    const T& operator*() const {
        HLS_PRAGMA(HLS inline);
        return _value;
    }

    T& operator*() {
        HLS_PRAGMA(HLS inline);
        return _value;
    }

    explicit operator bool() const {
        HLS_PRAGMA(HLS inline);
        return _valid;
    }
private:
    T _value;
    bool _valid;
};

template <typename T, typename U>
bool operator==(const optional<T>& a, const optional<U> b) {
    HLS_PRAGMA(HLS inline);
    return (!bool(a) && !bool(b)) || (bool(a) && bool(b) && *a == *b);
}

template <typename T, typename U>
bool operator!=(const optional<T>& a, const optional<U> b) {
    HLS_PRAGMA(HLS inline);
    return !(a == b);
}
