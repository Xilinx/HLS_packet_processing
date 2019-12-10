#include "catch.hpp"
#include "axi_stream_writer.hh"

#pragma GCC diagnostic ignored "-Wparentheses"

template <int D>
util::Bytes<D> keep_mask(const util::Bytes<D>& data, const ap_uint<D>& keep) {
    util::Bytes<D> result = data;
    for (int i = 0; i < D; i++) {
        if (!keep[i]) {
            result(util::BITS_PER_BYTE * (i + 1) - 1, util::BITS_PER_BYTE * i) = 0;
        }
    }
    return result;
}

// Checks to see if keep is continuous
template <int D>
bool check_keep(const ap_uint<D>& keep) {
    bool keeping = true;
    for (int i = 0; i < D; i++) {
        if (!keeping && keep[i]) return false;
        keeping = keep[i];
    }
    return true;
}

template <int D, typename... T>
void check_stream_contents(AxiStream<D>& stream, const util::Bytes<D> data, const ap_uint<D>& keep, bool last, T... args) {
    Axi<D> a = stream.read();

    // Require continous keeps (both real and expected in case the test is bad)
    CHECK(check_keep(a.keep));
    CHECK(check_keep(keep));

    CHECK(a.keep == keep);
    // Mask the data by the expected keep - we already checked that
    // the keeps are equal
    // This is because data without keep set is don't care
    util::Bytes<D> masked_actual = keep_mask(a.data, keep);
    util::Bytes<D> masked_expected = keep_mask(data, keep);
    CHECK(masked_actual == masked_expected);
    CHECK(a.last == last);

    check_stream_contents<D>(stream, args...);
}

template <int D>
void check_stream_contents(AxiStream<D>& stream) {
    CHECK(stream.empty());
};

TEST_CASE("axi_stream_writer_basic") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<8>("0x0706050403020100"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x0706050403020100"), ap_uint<8>("0xFF"), true
    );
}

TEST_CASE("axi_stream_writer_partial") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<3>("0x020100"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x020100"), ap_uint<8>("0x07"), true
    );
}

TEST_CASE("axi_stream_writer_partial_packing") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<3>("0x020100"), false);
    // Nothing should have been written yet
    check_stream_contents<8>(stream);
    writer.write_raw(util::Bytes<3>("0x050403"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x050403020100"), ap_uint<8>("0x3F"), true
    );
}

TEST_CASE("axi_stream_writer_multi") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<16>("0x0F0E0D0C0B0A09080706050403020100"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x0706050403020100"), ap_uint<8>("0xFF"), false,
        util::Bytes<8>("0x0F0E0D0C0B0A0908"), ap_uint<8>("0xFF"), true
    );
}

TEST_CASE("axi_stream_writer_multi_partial") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<12>("0x0B0A09080706050403020100"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x0706050403020100"), ap_uint<8>("0xFF"), false,
        util::Bytes<8>("0x0B0A0908"), ap_uint<8>("0x0F"), true
    );
}

TEST_CASE("axi_stream_writer_multi_partial_packing") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<7>("0x06050403020100"), false);
    writer.write_raw(util::Bytes<7>("0x0D0C0B0A090807"), true);
    check_stream_contents(stream,
        util::Bytes<8>("0x0706050403020100"), ap_uint<8>("0xFF"), false,
        util::Bytes<8>("0x0D0C0B0A0908"), ap_uint<8>("0x3F"), true
    );
}

TEST_CASE("axi_stream_writer_multi_partial_packing_strange_width") {
    AxiStream<3> stream;
    AxiStreamWriter<3> writer(stream);

    writer.write_raw(util::Bytes<7>("0x06050403020100"), false);
    writer.write_raw(util::Bytes<7>("0x0D0C0B0A090807"), true);
    check_stream_contents(stream,
        util::Bytes<3>("0x020100"), ap_uint<3>("0x7"), false,
        util::Bytes<3>("0x050403"), ap_uint<3>("0x7"), false,
        util::Bytes<3>("0x080706"), ap_uint<3>("0x7"), false,
        util::Bytes<3>("0x0B0A09"), ap_uint<3>("0x7"), false,
        util::Bytes<3>("0x000D0C"), ap_uint<3>("0x3"), true
    );
}

TEST_CASE("axi_stream_writer_zero_width") {
    AxiStream<8> stream;
    AxiStreamWriter<8> writer(stream);

    writer.write_raw(util::Bytes<7>("0x06050403020100"), false);
    writer.flush();

    writer.write_raw(util::Bytes<7>("0x0708090A0B0C0D"), false);
    writer.flush();

    writer.flush();

    check_stream_contents(stream,
        util::Bytes<8>("0x06050403020100"), ap_uint<8>("0x7F"), true,
        util::Bytes<8>("0x0708090A0B0C0D"), ap_uint<8>("0x7F"), true,
        util::Bytes<8>("0x00000000000000"), ap_uint<8>("0x00"), true
    );
}
