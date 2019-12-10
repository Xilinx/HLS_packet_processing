#include "catch.hpp"
#include "axi_stream_reader.hh"

#pragma GCC diagnostic ignored "-Wparentheses"

using namespace message;
using namespace LengthPolicy;

// Writes the bytes offset + 0 to offset + n * D - 1 in n groups
// of D bytes to the axi stream,
// setting the last flag on nth packet.
template <int D>
void send_test(AxiStream<D>& stream, int n, int offset = 0) {
    for (int x = 0; x < n; x++) {
        Axi<D> axi;
        for (int i = 0; i < D; i++) {
            axi.data(util::BITS_PER_BYTE * (i + 1) - 1, util::BITS_PER_BYTE * i) = offset + x * D + i;
        }
        axi.last = (x == n - 1);
        // Only sending whole beats
        axi.keep = ~ap_uint<D>(0);
        stream.write(axi);
    }
}

template <int W, int D>
void check_read_raw(AxiStreamReader<D>& reader, const util::Bytes<W> expected) {
    ReadResult<W> result = reader.template read_raw<Checked, W>();
    CHECK(result.len == W);
    CHECK(result.data == expected);
}

TEST_CASE("axi_stream_reader_basic") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    send_test<8>(stream, 1);

    check_read_raw<8>(reader, "0x0706050403020100");
}

TEST_CASE("axi_stream_reader_small_chunks") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    send_test<8>(stream, 2);

    for (int x = 0; x < 16; x++) {
        check_read_raw<1>(reader, x);
    }
}

TEST_CASE("axi_stream_reader_off_width") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);
    send_test<8>(stream, 3);

    // Read 7 bytes, then 10, which will force 2 more reads,
    // and make it combine data from a total of 3 reads.
    check_read_raw<7>(reader, "0x06050403020100");
    check_read_raw<10>(reader, "0x100F0E0D0C0B0A090807");
    // Make sure the remaining 7 bytes are correct
    check_read_raw<7>(reader, "0x17161514131211");
}

TEST_CASE("axi_stream_reader_drain") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);

    // Send 2 packets, drop the first, and read the second
    send_test<8>(stream, 1);
    send_test<8>(stream, 1, 8);
    reader.drain();
    check_read_raw<8>(reader, "0x0F0E0D0C0B0A0908");
}

TEST_CASE("axi_stream_reader_drain_partial_read") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);

    // Send 2 packets,
    // read the first a little bit, then drop it and read the secon
    send_test<8>(stream, 1);
    send_test<8>(stream, 1, 8);
    check_read_raw<4>(reader, "0x03020100");
    reader.drain();
    check_read_raw<8>(reader, "0x0F0E0D0C0B0A0908");
}

TEST_CASE("axi_stream_reader_drain_multi_partial_read") {
    AxiStream<8> stream;
    AxiStreamReader<8> reader(stream);

    // Send 2 packets,
    // read the first a little bit, then drop it and read the secon
    send_test<8>(stream, 3);
    send_test<8>(stream, 1, 24);
    check_read_raw<12>(reader, "0x0B0A09080706050403020100");
    reader.drain();
    check_read_raw<8>(reader, "0x1F1E1D1C1B1A1918");
}

TEST_CASE("axi_stream_reader_drain_multi_partial_read_strange_width") {
    AxiStream<3> stream;
    AxiStreamReader<3> reader(stream);

    // Send 2 packets,
    // read the first a little bit, then drop it and read the secon
    send_test<3>(stream, 3);
    send_test<3>(stream, 1, 9);
    check_read_raw<8>(reader, "0x0706050403020100");
    reader.drain();
    check_read_raw<3>(reader, "0x0B0A09");
}
