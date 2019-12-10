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
#include "util.hh"
#include "axi_stream_common.hh"
#include "message.hh"

/*
 * Writes arbitrary length data to an AXI stream.
 * This adheres to the Xilinx AXI Reference Guide UG 761: https://www.xilinx.com/support/documentation/ip_documentation/ug761_axi_reference_guide.pdf.
 * For more details, see AxiStreamReader (axi_stream_reader.hh).
 */
template <int DataWidth>
class AxiStreamWriter {
public:
    AxiStreamWriter(AxiStream<DataWidth>& stream) : stream_(stream) {
        HLS_PRAGMA(HLS inline);
        reset();
    }

    /*
     * Writes the specified message to the stream, and asserts the last signal on the last packet of the message
     * if last is set.
     */
    template <typename T>
    void write(const T& in, bool last) {
        HLS_PRAGMA(HLS inline);
        write_raw(T::to_raw(in), last);
    }

    /*
     * Writes the specified data to the stream.
     * The data must be an integer number of bytes.
     * If last is set, then asserts the last signal on the last packet written to the stream.
     *
     * Note: this takes an ap_uint instead of a util::Bytes such that template argument deduction works.
     */
    template <int MsgWidthBits>
    void write_raw(ap_uint<MsgWidthBits> msg, bool last) {
        HLS_PRAGMA(HLS inline);
        static_assert(MsgWidthBits % util::BITS_PER_BYTE == 0, "Type must be a integer number of bytes");
        const int MsgWidth = MsgWidthBits / util::BITS_PER_BYTE;

        // Maximum number of beats we might buffer or send = ceil((MsgWidth + DataWidth - 1) / DataWidth)
        // = ceil((MsgWidth - 1) / DataWidth) + 1
        // Uses the fact that ceil(a / b) = (a + b - 1) / b under integer division and without overflow.
        const int max_beats = (MsgWidth + DataWidth - 2) / DataWidth + 1;

        // Number of bytes which still have to be either buffered or sent
        util::uint_t<MsgWidth> remaining = MsgWidth;

        for (int i = 0; i < max_beats; i++) {
            HLS_PRAGMA(HLS unroll);
            // Not including this if statement in the loop guard allows Vivado to generate better
            // code because then the loop has a fixed number of iterations.
            if (remaining != 0) {
                // Put the low bytes of msg into the high bytes (unused bytes) of the buffer
                util::Bytes<DataWidth> to_add = util::Bytes<DataWidth>(msg) << (len_ * util::BITS_PER_BYTE);
                buf_ |= to_add;
                // Figure out how many bytes were added
                Len bytes_added = DataWidth - len_;
                if (bytes_added > remaining) {
                    bytes_added = remaining;
                }
                msg >>= bytes_added * util::BITS_PER_BYTE;
                remaining -= bytes_added;
                len_ += bytes_added;

                // If we have a full frame, or the final frame, send it
                bool is_last = remaining == 0 && last;
                if (len_ == DataWidth || is_last) {
                    Axi<DataWidth> dout;
                    dout.data = buf_;
                    dout.last = is_last;
                    dout.keep = (~ap_uint<DataWidth>(0)) >>= (DataWidth - len_);
                    stream_.write(dout);
                    reset();
                }
            }
        }
    }

    void flush() {
        HLS_PRAGMA(HLS inline);
        Axi<DataWidth> dout;
        dout.data = buf_;
        dout.last = true;
        dout.keep = (~ap_uint<DataWidth>(0)) >>= (DataWidth - len_);
        stream_.write(dout);
        reset();
    }

private:
    void reset() {
        HLS_PRAGMA(HLS inline);
        len_ = 0;
        buf_ = 0;
    }

    using Len = util::uint_t<DataWidth>;

    // low len_ bytes are used
    util::Bytes<DataWidth> buf_;
    Len len_;

    AxiStream<DataWidth>& stream_;
};
