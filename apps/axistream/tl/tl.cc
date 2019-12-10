#include <cstdint>
#include "tl.hh"
#include "axi_stream_reader.hh"
#include "axi_stream_writer.hh"

void tl(AxiStream<8>& in_stream, AxiStream<8>& output_stream) {
    HLS_PRAGMA(HLS interface port=in_stream axis);
    HLS_PRAGMA(HLS interface port=output_stream axis);
    HLS_PRAGMA(HLS interface ap_ctrl_none port=return);

    AxiStreamReader<8> reader(in_stream);

    uint16_t total = 0;
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        HLS_PRAGMA(HLS pipeline II=1);
        input in = reader.read<LengthPolicy::Unchecked, input>();
        total += in.get<input_m::value>();
    }

    output out;
    out.set<output_m::value>(total);
    AxiStreamWriter<8> writer(output_stream);
    writer.write(out, true);
}
