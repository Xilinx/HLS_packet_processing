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

    header h_read;
    h_read = reader.read<header>();
    CHECK(h_read == m1.get<h1>());
    message_1 m1_read = reader.read_after<message_1>(h_read);
    CHECK(m1_read == m1);
    reader.drain();

    h_read = reader.read<header>();
    CHECK(h_read == m2.get<h2>());
    message_2 m2_read = reader.read_after<message_2>(h_read);
    CHECK(m2_read == m2);
}

