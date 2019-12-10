#include "catch.hpp"
#include "tl.hh"
#include "message.hh"
#include "axi_stream_reader.hh"
#include "axi_stream_writer.hh"

TEST_CASE("tl") {
    AxiStream<8> input_stream;
    AxiStream<8> output_stream;
    AxiStreamWriter<8> writer(input_stream);
    AxiStreamReader<8> reader(output_stream);

    uint16_t expected = 0;
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        input in;
        in.set<input_m::value>(i);
        writer.write(in, i == NUM_INPUTS - 1);
        expected += i;
    }

    tl(input_stream, output_stream);

    output out = reader.read<LengthPolicy::Unchecked, output>();
    reader.drain();

    CHECK(out.get<output_m::value>() == expected);
}
