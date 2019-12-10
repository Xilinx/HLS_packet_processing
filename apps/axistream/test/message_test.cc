#include "catch.hpp"
#include "message.hh"
#include "array.hh"
#include "util.hh"

using namespace message;

/*
 * struct t1 {
 *     uint16_t f1; // Little endian
 *     uint16_t f2; // Big endian
 * };
 * struct t2 {
 *     t1 g1[2];
 *     t1 g2;
 *     uint16_t g3; // Little endian
 * };
 */
struct f1 : lu16 {};
struct f2 : bu16 {};
using t1 = Struct<f1, f2>::type;

struct g1 : Array<t1, 2> {};
struct g2 : t1 {};
struct g3 : lu16 {};
using t2 = Struct<g1, g2, g3>::type;

TEST_CASE("message_basic") {
    t1 x;
    x.set<f1>(0xABCD);
    x.set<f2>(0x1234);
    CHECK(x.get<f1>() == 0xABCD);
    CHECK(x.get<f2>() == 0x1234);
    CHECK(x.get_raw<f1>() == 0xABCD);
    // f2 is big-endian so the raw representation is flipped
    CHECK(x.get_raw<f2>() == 0x3412);
    // The first field is in the least significant location
    CHECK(t1::to_raw(x) == 0x3412ABCD);

    t2 y;
    array<t1, 2> z;
    z[0] = x;
    x.set<f1>(0xBEEF);
    x.set<f2>(0xDEAD);
    z[1] = x;
    y.set<g1>(z);
    x.set<f1>(0xCAFE);
    x.set<f2>(0xBABE);
    y.set<g2>(x);
    y.set<g3>(0xF00D);

    CHECK(y.get<g1>()[0].get<f1>() == 0xABCD);
    CHECK(y.get<g1>()[0].get<f2>() == 0x1234);
    CHECK(y.get<g1>()[1].get<f1>() == 0xBEEF);
    CHECK(y.get<g1>()[1].get<f2>() == 0xDEAD);
    CHECK(y.get<g2>().get<f1>() == 0xCAFE);
    CHECK(y.get<g2>().get<f2>() == 0xBABE);
    CHECK(y.get<g3>() == 0xF00D);
    CHECK(t2::to_raw(y) == util::Bytes<14>("0xF00DBEBACAFEADDEBEEF3412ABCD"));
}

TEST_CASE("message_make") {
    t1 x = t1::make(0xABCD, 0x1234);
    CHECK(x.get<f1>() == 0xABCD);
    CHECK(x.get<f2>() == 0x1234);
}
