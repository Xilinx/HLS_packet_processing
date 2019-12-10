#include "catch.hpp"
#include "axi_stream_reader.hh"
#include "axi_stream_writer.hh"


using namespace message;

/*
 * struct header {
 *     uint16_t message_type;
 * };
 * struct message_1 {
 *     header h1;
 *     uint16_t value;
 * };
 * struct message_2 {
 *     header h2;
 * };
 */
struct message_type : lu16 {};
using header = Struct<message_type>::type;

struct h1 : header {};
struct value : lu16 {};
using message_1 = Struct<h1, value>::type;

struct h2 : header {};
using message_2 = Struct<h2, value>::type;

TEST_CASE("axi_stream_integeration_read_after") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    header h;
    message_1 m1;
    h.set<message_type>(1);
    m1.set<h1>(h);
    m1.set<value>(42);
    message_2 m2;
    h.set<message_type>(2);
    m2.set<h2>(h);

    writer.write(m1, true);
    writer.write(m2, true);

    optional<header> h_read = reader.read<LengthPolicy::Checked, header>();
    CHECK(bool(h_read));
    CHECK(*h_read == m1.get<h1>());
    optional<message_1> m1_read = reader.read_after<LengthPolicy::Checked, message_1>(*h_read);
    CHECK(bool(m1_read));
    CHECK(*m1_read == m1);
    reader.drain();

    h_read = reader.read<LengthPolicy::Checked, header>();
    CHECK(bool(h_read));
    CHECK(*h_read == m2.get<h2>());
    optional<message_2> m2_read = reader.read_after<LengthPolicy::Checked, message_2>(*h_read);
    CHECK(bool(m2_read));
    CHECK(*m2_read == m2);
}

TEST_CASE("axi_stream_integration_read_checked") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    // Send 2 1-byte packets. Each will take an entire 8-byte data beat.
    // So 2 8-byte data beats will be sent
    writer.write_raw(util::Bytes<1>(0xAB), true);
    writer.write_raw(util::Bytes<1>(0xCD), true);

    ReadResult<16> result;

    // Takes 2 reads, 1 per packet!
    result = reader.read_raw<LengthPolicy::Checked, 16>();
    CHECK(result.data == 0xAB);
    CHECK(result.len == 1);

    // Check that at the end of the packet, we can't read anymore until a drain()
    result = reader.read_raw<LengthPolicy::Checked, 16>();
    CHECK(result.len == 0);
    reader.drain();

    result = reader.read_raw<LengthPolicy::Checked, 16>();
    CHECK(result.data == 0xCD);
    CHECK(result.len == 1);
}

TEST_CASE("axi_stream_integration_read_unchecked") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    // Send 2 1-byte packets. Each will take an entire 8-byte data beat.
    // So 2 8-byte data beats will be sent
    writer.write_raw(util::Bytes<1>(0xAB), true);
    writer.write_raw(util::Bytes<1>(0xCD), true);

    // Read 16 bytes _unchecked_! This will ignore the last and keep flags,
    // and combine the 2 packets.
    util::Bytes<16> result = reader.read_raw<LengthPolicy::Unchecked, 16>();
    // Can't check any other bytes -- they are undefined! We only set these 2 bytes.
    CHECK(result(7, 0) == 0xAB);
    CHECK(result(71, 64) == 0xCD);
}

TEST_CASE("axi_stream_integration_read_raw_checked_multi") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<1>(0xAB), false);
    writer.write_raw(util::Bytes<2>(0xCDEF), false);
    writer.write_raw(util::Bytes<8>(0xdeadbeef), true);

    ReadResult<1> a;
    ReadResult<2> b;
    ReadResult<8> c;
    reader.read_raw<LengthPolicy::Checked>(a, b, c);

    CHECK(a.data == 0xAB);
    CHECK(a.len == 1);
    CHECK(b.data == 0xCDEF);
    CHECK(b.len == 2);
    CHECK(c.data == 0xdeadbeef);
    CHECK(c.len == 8);
}

TEST_CASE("axi_stream_integration_read_raw_unchecked_multi") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<1>(0xAB), false);
    writer.write_raw(util::Bytes<2>(0xCDEF), false);
    writer.write_raw(util::Bytes<8>(0xdeadbeef), true);

    util::Bytes<1> a;
    util::Bytes<2> b;
    util::Bytes<8> c;
    reader.read_raw<LengthPolicy::Unchecked, 1, 2, 8>(a, b, c);

    CHECK(a == 0xAB);
    CHECK(b == 0xCDEF);
    CHECK(c == 0xdeadbeef);
}

TEST_CASE("axi_stream_integeration_read_checked_multi") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    AxiStreamWriter<8> writer(stream);

    header h;
    message_1 m1;
    h.set<message_type>(1);
    m1.set<h1>(h);
    m1.set<value>(42);

    message_1 m2 = m1;
    m2.set<value>(43);

    writer.write(m1, false);
    writer.write(m2, true);

    optional<message_1> m1_read;
    optional<message_1> m2_read;
    optional<message_1> m3_read;
    reader.read<LengthPolicy::Checked>(m1_read, m2_read, m3_read);
    CHECK(bool(m1_read));
    CHECK(*m1_read == m1);
    CHECK(bool(m2_read));
    CHECK(*m2_read == m2);
    CHECK_FALSE(bool(m3_read));
}

