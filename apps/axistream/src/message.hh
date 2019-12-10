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

#include <type_traits>
#include <cstdint>
#include "hls_macros.hh"
#include "hls_int.hh"
#include "array.hh"
#include "util.hh"

/*
 * Contrains a series of classes to support interpretation of bit vectors.
 *
 * Example usage:
 * Consider the C struct:
 * struct MyStruct {
 *     uint64_t a;
 *     int32_t b;
 * };
 *
 * This struct can be represented in the message framework as:
 * struct A : Int<64, Sign::UNSIGNED, Endianness::LITTLE> {};
 * struct B : Int<32, Sign::SIGNED, Endianness::LITTLE> {};
 * using MyStruct = Struct<A, B>;
 *
 * It can then be used as:
 * MyStruct x;
 * x.set<A>(3);
 * ap_uint<64> y = x.get<B>();
 *
 * All messages are backed by a bitvector, x.data.
 * In this case, if the user received a packed 96 bit vector,
 * they can unpack it into the struct with:
 * x = MyStruct::from_raw(<input data>)
 * Then, they can easily get and set to fields within this packed representation.
 * There is no overhead to this at all - it is equivlent to breaking the input vector
 * up into local variables manually and loading them into the C struct described above.
 *
 * In particular, these messages can be easily used with the AxiStreamReader and AxiStreamWriter classes.
 * The read/write methods enable reading/writing directly to/from an AXI stream into/from message types.
 *
 * For convience, the types i8, li16, li32, li64, bi16, bi32, bi64, i8, li16, li32, li64, bi16, bi32 and bi64
 * are provided. These types are integers of the specified length, sign (indicated by i or u), and endianness
 * (inidicated by l or b).
 * Using templates are also provided for li<int W>, lu<int W>, bi<int W>, bu<int W>, lx<int W, Sign S>, and
 * bx<int W, Sign s>.
 *
 * Both nesting and unions are supported. For example:
 *
 * struct First : MyStruct {};
 * struct Second : Int<32, Sign::UNSIGNED, Endianness::LITTLE> {};
 * using MyPair = Struct<First, Second>;
 *
 * The MyPair struct can then be used as:
 * MyPair y;
 * y.set<First>(x);
 * y.set<Second>(3);
 *
 * MyStruct z = y.get<First>();
 *
 * Unions are also supported, and work as C unions do.
 * All fields have an offset of 0, and thus overlap.
 * The size of the union is the size of the biggest field.
 *
 * Enums are supported as well:
 * enum class MyEnum : uint8_t {
 *     THING1,
 *     THING2
 * };
 *
 * struct MyEnumField : Enum<MyEnum, Endianness::LITTLE> {};
 *
 * Arrays are also supported:
 * struct MyArrayField : Array<Int<8, Sign::UNSIGNED, Endianness::LITTLE, 10>> {};
 *
 * The array type provides operator[] to get and set elements within the array.
 *
 * In general:
 * * Create fields by subclassing their type
 * * Create types by defining alias with a using statement
 */
namespace message {

enum class Sign {
    UNSIGNED, SIGNED
};

enum class Endianness {
    BIG, LITTLE
};

template <typename T>
T flip_endianness(const T& x) {
    static_assert(T::width % util::BITS_PER_BYTE == 0, "Type must be a integer number of bytes");
    HLS_PRAGMA(HLS inline);
    const int num_bytes = T::width / util::BITS_PER_BYTE;
    T out = 0;
    for (int i = 0; i < num_bytes; i++) {
        HLS_PRAGMA(HLS unroll);
        int j = num_bytes - i - 1;
        out(util::BITS_PER_BYTE * (i + 1) - 1, util::BITS_PER_BYTE * i) = x(util::BITS_PER_BYTE * (j + 1) - 1, util::BITS_PER_BYTE * j);
    }
    return out;
}

/*
 * Endianness Converter
 */
template <Endianness E>
struct EndiannessConverter {};

template <>
struct EndiannessConverter<Endianness::LITTLE> {
    template <int W>
    static ap_uint<W> convert(const ap_uint<W>& x) {
        HLS_PRAGMA(HLS inline);
        return x;
    }

};

template <>
struct EndiannessConverter<Endianness::BIG> {
    template <int W>
    static ap_uint<W> convert(const ap_uint<W>& x) {
        HLS_PRAGMA(HLS inline);
        return flip_endianness(x);
    }
};

/*
 * Helper type to select between ap_uint and ap_int
 */
template <int W, Sign S>
struct TypeSelector {};

template <int W>
struct TypeSelector<W, Sign::SIGNED> {
    typedef ap_int<W> type;
};

template <int W>
struct TypeSelector<W, Sign::UNSIGNED> {
    typedef ap_uint<W> type;
};

template <> struct TypeSelector<8, Sign::SIGNED> { typedef int8_t type; };
template <> struct TypeSelector<16, Sign::SIGNED> { typedef int16_t type; };
template <> struct TypeSelector<32, Sign::SIGNED> { typedef int32_t type; };
template <> struct TypeSelector<64, Sign::SIGNED> { typedef int64_t type; };
template <> struct TypeSelector<8, Sign::UNSIGNED> { typedef uint8_t type; };
template <> struct TypeSelector<16, Sign::UNSIGNED> { typedef uint16_t type; };
template <> struct TypeSelector<32, Sign::UNSIGNED> { typedef uint32_t type; };
template <> struct TypeSelector<64, Sign::UNSIGNED> { typedef uint64_t type; };

/*
 * Type definitions. All types must define:
 * width: the width in bits
 * raw: the type of a raw field - ap_uint<width>. Note that even
 * though the raw value of a field might have the same type as the nominal
 * type (given by type, below), it may not be equal in value.
 * For example, the endianness may be flipped.
 * type: the type of a field when used normally. For integer fields,
 * this is ap_int<width> or ap_uint<width>. For enum fields,
 * this is the enum type. For composite fields (fields with a struct
 * or union type), this is a message of that type.
 * to_raw: function from the nominal type to the raw type
 * from_raw: function from the raw type to the nominal type
 */

template <int W, Sign S, Endianness E>
struct Int {
    static const int width = W;

    typedef ap_uint<width> raw;
    typedef typename TypeSelector<W, S>::type type;

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        return EndiannessConverter<E>::convert(raw(x));
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        return EndiannessConverter<E>::convert(x);
    }
};

template <int W, Sign S> using lx = Int<W, S, Endianness::LITTLE>;
template <int W, Sign S> using bx = Int<W, S, Endianness::BIG>;

template <int W> using li = lx<W, Sign::SIGNED>;
template <int W> using lu = lx<W, Sign::UNSIGNED>;
template <int W> using bi = bx<W, Sign::SIGNED>;
template <int W> using bu = bx<W, Sign::UNSIGNED>;

using i8 = li<8>;
using li16 = li<16>;
using li32 = li<32>;
using li64 = li<64>;
using bi16 = bi<16>;
using bi32 = bi<32>;
using bi64 = bi<64>;

using u8 = lu<8>;
using lu16 = lu<16>;
using lu32 = lu<32>;
using lu64 = lu<64>;
using bu16 = bu<16>;
using bu32 = bu<32>;
using bu64 = bu<64>;

struct Bool {
    static const int width = 1;

    typedef ap_uint<width> raw;
    typedef bool type;

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        return raw(x);
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        return type(x);
    }
};

struct Char {
    static const int width = util::BITS_PER_BYTE;

    typedef ap_uint<width> raw;
    typedef char type;

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        return raw(x);
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        return type(x);
    }
};

// std::underlying_type does not translate,
// but vivado accepts this, which is probably equivelent.
template <typename T>
struct underlying_type_w {
    typedef typename std::conditional<std::is_signed<T>::value, typename std::make_signed<T>::type, typename std::make_unsigned<T>::type>::type type;
};

template <typename T, Endianness E, int W = util::BITS_PER_BYTE * sizeof(typename underlying_type_w<T>::type)>
struct Enum {
    static_assert(std::is_enum<T>::value, "T must be an enum");
    static const int width = W;

    typedef ap_uint<width> raw;
    typedef T type;

    typedef typename underlying_type_w<T>::type underlying;

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        return EndiannessConverter<E>::convert(raw(static_cast<underlying>(x)));
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        return T(static_cast<underlying>(EndiannessConverter<E>::convert(x)));
    }
};

/*
 * Simple array type for use with messages.
 * T is the element type, and must be another message type (either Int, Enum, Struct, Union, or Array)
 * N is the number of elements.
 */
template <typename T, int N>
struct Array {
    static const int width = T::width * N;

    typedef ap_uint<width> raw;
    typedef array<typename T::type, N> type;

    static const int size = N;

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        raw result(0);
        for (int i = 0; i < N; i++) {
            HLS_PRAGMA(HLS unroll);
            result(T::width * (i + 1) - 1, T::width * i) = T::to_raw(x[i]);
        }
        return result;
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        type result;
        for (int i = 0; i < N; i++) {
            HLS_PRAGMA(HLS unroll);
            typename T::raw slice = x(T::width * (i + 1) - 1, T::width * i);
            result[i] = T::from_raw(slice);
        }
        return result;
    }
};


/*
 * Helper type to find the width of a list of fields
 */
template <typename H, typename... T>
struct StructWidth {
    static const int width = H::width + StructWidth<T...>::width;
};

template <typename H>
struct StructWidth<H> {
    static const int width = H::width;
};

/*
 * Helper type to determine the offset of a field in a struct.
 * F is the field type, and the list H, T... are the fields.
 */
template <typename F, typename H, typename... T>
struct StructOffsetOf {
    static const int offset = H::width + StructOffsetOf<F, T...>::offset;
};

template <typename F, typename... T>
struct StructOffsetOf<F, F, T...> {
    static const int offset = 0;
};

struct StructContainer {
    template <typename... T>
    using Width = StructWidth<T...>;

    template <typename F, typename... T>
    using OffsetOf = StructOffsetOf<F, T...>;
};

/*
 * Helper type to find the maximum width width of a list of fields
 */
template <typename H, typename... T>
struct UnionWidth {
    static const int width = H::width > UnionWidth<T...>::width ? H::width : UnionWidth<T...>::width;
};

template <typename H>
struct UnionWidth<H> {
    static const int width = H::width;
};
/*
 * Helper type to determine the offset of a field in a union.
 * For any field that is actually in the union, the offset is 0,
 * and if the field isn't in the union this won't compile
 */
template <typename F, typename H, typename... T>
struct UnionOffsetOf {
    static const int offset = UnionOffsetOf<F, T...>::offset;
};

template <typename F, typename... T>
struct UnionOffsetOf<F, F, T...> {
    static const int offset = 0;
};

struct UnionContainer {
    template <typename... T>
    using Width = UnionWidth<T...>;

    template <typename F, typename... T>
    using OffsetOf = UnionOffsetOf<F, T...>;
};

template <typename M, typename... F>
struct Setter {};

template <typename M, typename H, typename... F>
struct Setter<M, H, F...> {
    template <typename TH, typename... T>
    static void set(M& m, const TH& value, const T&... rest) {
        HLS_PRAGMA(HLS inline);
        m.template set<H>(value);
        Setter<M, F...>::set(m, rest...);
    }
};

template <typename M>
struct Setter<M> {
    static void set(M&) {
        HLS_PRAGMA(HLS inline);
    }
};

template <typename C, typename... T>
struct Message {
    static const int width = C::template Width<T...>::width;
    typedef Message<C, T...> type;

    typedef ap_uint<width> raw;
    raw data;

    explicit Message(const raw& value = 0) : data(value) {
        HLS_PRAGMA(HLS inline);
    }

    explicit operator raw() const {
        HLS_PRAGMA(HLS inline);
        return type::to_raw(*this);
    }

    /*
     * Writes the specified value into the field.
     */
    template <typename F>
    void set_raw(const typename F::raw& value) {
        HLS_PRAGMA(HLS inline);
        data(F::width + C::template OffsetOf<F, T...>::offset - 1, C::template OffsetOf<F, T...>::offset) = value;
    }

    /*
     * Reads the specified field as a raw value
     */
    template <typename F>
    typename F::raw get_raw() const {
        HLS_PRAGMA(HLS inline);
        return data(F::width + C::template OffsetOf<F, T...>::offset - 1, C::template OffsetOf<F, T...>::offset);
    }

    /*
     * Writes the specified field with a value. Since the field is not primitive,
     * the value must be a Message -- it must have the same type as the field.
     */
    template <typename F>
    void set(const typename F::type& value) {
        HLS_PRAGMA(HLS inline);
        set_raw<F>(F::to_raw(value));
    }

    /*
     * Reads a field as a message of the correct type
     */
    template <typename F>
    typename F::type get() const {
        HLS_PRAGMA(HLS inline);
        return F::from_raw(get_raw<F>());
    }

    raw to_raw() const {
        HLS_PRAGMA(HLS inline);
        return type::to_raw(*this);
    }

    bool operator==(const type& other) const {
        HLS_PRAGMA(HLS inline);
        return data == other.data;
    }

    bool operator!=(const type& other) const {
        HLS_PRAGMA(HLS inline);
        return data != other.data;
    }

    template <typename... A>
    static type make(const A&... args) {
        HLS_PRAGMA(HLS inline);
        type result;
        Setter<type, T...>::set(result, args...);
        return result;
    }

    static raw to_raw(const type& x) {
        HLS_PRAGMA(HLS inline);
        return x.data;
    }

    static type from_raw(const raw& x) {
        HLS_PRAGMA(HLS inline);
        return type(x);
    }
};

/*
 * While the using declarations below are nicer than the structs
 * below, (because then you can type Struct<stuff> instead of
 * Struct<stuff>::type), Vivado crashes if you use them.
 *
 * template <typename... T>
 * using Struct = Message<StructContainer, T...>;
 *
 * template <typename... T>
 * using Union = Message<UnionContainer, T...>;
*/

template <typename... T>
struct Struct {
    typedef Message<StructContainer, T...> type;
};
template <typename... T>
struct Union {
    typedef Message<UnionContainer, T...> type;
};

}
