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
#include "hls_macros.hh"
#include "util.hh"
#include "axi_stream_common.hh"
#include "message.hh"
#include "optional.hh"

template <int B>
struct ReadResult {
    util::Bytes<B> data;
    util::uint_t<B> len;
};

// Length policys control how the AxiStreamReader handles the keep/last flags on AXI streams
// A LengthPolicy must have the following:
// * static bool more_available(bool last)
// * template <int MsgWidth> using Result
// * template <int MsgWidth> static Result<MsgWidth> result(util::Bytes<MsgWidth> data, util::uint_t<MsgWidth> len)
// * template <typename T> using TypedResult
// * template <typename T, int MsgWidth> static TypedResult<T> typed_result(Result<MsgWidth> read_result)
// * template <int PrefixWidth, int RemainderWidth> static Result<PrefixWidth + RemainderWidth> prefix_with_raw(util::Bytes<PrefixWidth> prefix, Result<RemainderWidth> read_result)
// * template <int W, int T> static Result<W> sub_result(Result<T> result, util::uint_t<T> start_index)
//
// Note that all template parameters are declared on the methods, and not on the struct. This is so a LengthPolicy can be used without specifying parameters.
namespace LengthPolicy {

// Checked means that the keep/last flags are handled exactly, and the stream reader will return
// both the data read and the length of the data
struct Checked {
    static bool more_available(bool last) {
        HLS_PRAGMA(HLS inline);
        return !last;
    }

    template <int MsgWidth>
    using Result = ReadResult<MsgWidth>;

    template <int MsgWidth>
    static Result<MsgWidth> result(util::Bytes<MsgWidth> data, util::uint_t<MsgWidth> len) {
        HLS_PRAGMA(HLS inline);
        return { .data = data, .len = len };
    }

    template <typename T>
    using TypedResult = optional<T>;

    template <typename T, int MsgWidth>
    static TypedResult<T> typed_result(Result<MsgWidth> read_result) {
        HLS_PRAGMA(HLS inline);
        if (read_result.len == MsgWidth) {
            return T::from_raw(read_result.data);
        } else {
            return nullopt();
        }
    }

    template <int PrefixWidth, int RemainderWidth>
    static Result<PrefixWidth + RemainderWidth> prefix_with_raw(util::Bytes<PrefixWidth> prefix, Result<RemainderWidth> read_result) {
        HLS_PRAGMA(HLS inline);
        return { .data = (read_result.data, prefix), .len = read_result.len + PrefixWidth };
    }

    template <int W, int T>
    static Result<W> sub_result(Result<T> result, util::uint_t<T> start_index) {
        HLS_PRAGMA(HLS inline);
        if (start_index > result.len) {
            return { .data = 0, .len = 0 };
        } else {
            util::uint_t<W> len;
            if (start_index + W <= result.len) {
                len = W;
            } else {
                len = result.len - start_index;
            }
            return { .data = result.data >> (start_index * util::BITS_PER_BYTE), .len = len };
        }
    }
};

// Unchecked means that keep/last are ignored - this is potentially unsafe.
// If the steram does not have enough data, an Unchecked read with an AxiStreamReader
// will read past the last flag, into the next packet, to satisfy the read. It will also read
// bytes which are masked off by the keep mask.
// However, if you are sure that the data is there, using Unchecked will generate more efficient hardware,
// as correct keep/last checking requires both extra logic, and complicates the stream-reading loop because
// the loop exit condition becomes data-dependant.
struct Unchecked {
    static bool more_available(bool) {
        HLS_PRAGMA(HLS inline);
        return true;
    }

    template <int MsgWidth>
    using Result = util::Bytes<MsgWidth>;

    template <int MsgWidth>
    static Result<MsgWidth> result(util::Bytes<MsgWidth> data, util::uint_t<MsgWidth>) {
        HLS_PRAGMA(HLS inline);
        return data;
    }

    template <typename T>
    using TypedResult = T;

    template <typename T, int MsgWidth>
    static TypedResult<T> typed_result(Result<MsgWidth> read_result) {
        HLS_PRAGMA(HLS inline);
        return T::from_raw(read_result);
    }

    template <int PrefixWidth, int RemainderWidth>
    static Result<PrefixWidth + RemainderWidth> prefix_with_raw(util::Bytes<PrefixWidth> prefix, Result<RemainderWidth> read_result) {
    HLS_PRAGMA(HLS inline);
        return (read_result, prefix);
    }

    template <int W, int T>
    static Result<W> sub_result(Result<T> result, util::uint_t<T> start_index) {
        HLS_PRAGMA(HLS inline);
        return result((start_index + W) * util::BITS_PER_BYTE - 1, start_index * util::BITS_PER_BYTE);
    }
};
}

/*
 * Reads arbitrary length data (in bytes) from an AXI stream.
 * This adheres to the Xilinx AXI Reference Guide UG 761:
 * https://www.xilinx.com/support/documentation/ip_documentation/ug761_axi_reference_guide.pdf
 *
 * This standard differs from the ARM standard
 * (https://static.docs.arm.com/ihi0051/a/IHI0051A_amba4_axi4_stream_v1_0_protocol_spec.pdf)
 * in the following key ways:
 * TKEEP: is ignored, since null bytes are only used in a packet with TLAST set, and the user of this API requests a packet by length.
 * TID, TDEST, TUSER: are all currently ignored, even though Xilinx supports them. Note that the ap_axiu type requires a bit width of at
 * least 1 for all of these signals, even though they are unused, in accordance with UG 761.
 * (Also, the tools won't accept 0 bit signals in Verilog anyway, but will delete unused 1 bit signals).
 */
template <int DataWidth>
class AxiStreamReader {
public:
    AxiStreamReader(AxiStream<DataWidth>& stream) : stream_(stream) {
        HLS_PRAGMA(HLS inline);
        reset();
    }

    /**
     * Reads all remaining data from the stream until the last packet.
     */
    void drain() {
        HLS_PRAGMA(HLS inline);
    DRAIN:
        while (!last_) {
            HLS_PRAGMA(HLS loop_tripcount min=0);
            last_ = stream_.read().last;
        }
        reset();
    }

    /*
     * Reads a sequence of messages from a stream. Example:
     * message1 m1;
     * message2 m2;
     * stream.read<LengthPolicy::Unchecked>(m1, m2);
     */
    template <typename LengthPolicy, typename H, typename... T>
    void read(typename LengthPolicy::template TypedResult<H>& out, typename LengthPolicy::template TypedResult<T>&... rest) {
        HLS_PRAGMA(HLS inline);
        const int total = util::Sum<H::width, T::width...>::value / util::BITS_PER_BYTE;
        assign_sub_result_typed<LengthPolicy, total, H, T...>(read_raw<LengthPolicy, total>(), 0, out, rest...);
    }

    /*
     * Reads a message::Struct from the stream
     */
    template <typename LengthPolicy, typename T>
    typename LengthPolicy::template TypedResult<T> read() {
        HLS_PRAGMA(HLS inline);
        typename LengthPolicy::template TypedResult<T> out;
        read<LengthPolicy, T>(out);
        return out;
    }

    /*
     * Given some message T which starts with header H, reads all non-header bytes
     * of T from the stream, then concatenates the new bytes with the given header
     * to return a complate T.
     *
     * This overload works for types T which are strictly larger than the headers.
     */
    template <typename LengthPolicy, typename T, typename H>
    typename std::enable_if<(T::width > H::width), typename LengthPolicy::template TypedResult<T>>::type read_after(const H& header) {
        HLS_PRAGMA(HLS inline);
        static_assert(H::width % util::BITS_PER_BYTE == 0, "Header must be an integer number of bytes");
        static_assert(T::width % util::BITS_PER_BYTE == 0, "Total must be an integer number of bytes");

        const int to_read = T::width - H::width;
        const int to_read_num_bytes = to_read / util::BITS_PER_BYTE;
        const int total_num_bytes = T::width / util::BITS_PER_BYTE;
        const int header_num_bytes = H::width / util::BITS_PER_BYTE;

        auto read_result =
            LengthPolicy::template prefix_with_raw<header_num_bytes, to_read_num_bytes>(H::to_raw(header), read_raw<LengthPolicy, to_read_num_bytes>());
        return LengthPolicy::template typed_result<T, total_num_bytes>(read_result);
    }

    /*
     * Given some message T which starts with header H, reads all non-header bytes
     * of T from the stream, then concatenates the new bytes with the given header
     * to return a complate T.
     *
     * This overload works for types T which are the same size as the header - in other words
     * the type consists of only a header.
     */
    template <typename LengthPolicy, typename T, typename H>
    typename std::enable_if<T::width == H::width, T>::type read_after(const H& header) {
        HLS_PRAGMA(HLS inline);
        return T::from_raw(H::to_raw(header));
    }


    /*
     * Reads the specified number of byes from the stream.
     */
    template <typename LengthPolicy, int MsgWidth>
    typename LengthPolicy::template Result<MsgWidth> read_raw() {
        HLS_PRAGMA(HLS inline);
        // The size of the working buffer.
        // This handles the worst-case where we have DataWidth - 1 bytes in buf_,
        // and so the last byte of the message data will be at byte index DataWidth - 1 + MsgWidth - 1
        // (we will need to store DataWidth - 1 + MsgWidth bytes).
        // The work size, in the last data beat read, the high byte will not be part of this message,
        // but we still must store it in the work buffer.
        // Thus the work buffer neesd to be DataWidth + MsgWidth bytes.
        const int work_size = util::BITS_PER_BYTE * (DataWidth + MsgWidth);
        // Maximum number of beats required to get the whole message = ceil(MsgWidth / DataWidth)
        // It is possible the last beat isn't needed depending on how much is stored in buf_
        // from the last read
        const int max_beats = (MsgWidth + DataWidth - 1) / DataWidth;
        // Maximum number of new bytes we might receive
        const int max_received = DataWidth * max_beats;
        // Type which can store the number of bytes to shift the final result by
        // At most, all the low DataWidth bytes need to be shifted
        using Shift = util::uint_t<DataWidth * util::BITS_PER_BYTE>;

        // Buf will be zero extended since work is MsgWidth longer than buf_
        ap_uint<work_size> work = buf_;
        // Represents the number of bytes we have so far
		using Received = util::uint_t<max_received>;
        Received received = len_;
        Received received_upper_bound = len_;
        // Represents the index in work where the last byte of the message will be written
        util::state_t<work_size> last_index = DataWidth - len_ + MsgWidth - 1;

        for (int i = 0; i < max_beats; i++) {
            HLS_PRAGMA(HLS unroll);
            // Not including this if statement in the loop guard allows Vivado to generate better
            // code because then the loop has a fixed number of iterations.
            // Only check received_upper_bound - not the actual received!
            // This is because received_upper_bound == received _unless_ last_ is set --
            // each data beat contains DataWidth bytes unless last_ is set
            // Using received_upper_bound here instead of received helps the HLS tool
            // because then it is evident that the loop only exits on last_,
            // and not the uncontrained output of compute_received_delta.
			if (received_upper_bound >= Received(MsgWidth) || !LengthPolicy::more_available(last_)) {
                break;
            }

            Axi<DataWidth> din = stream_.read();
            buf_ = din.data;
            last_ = din.last;

            // This assignment will truncate the most significant bits
            ap_uint<work_size> shifted = ap_uint<work_size>(din.data) << (util::BITS_PER_BYTE * (i + 1) * DataWidth);
            work |= shifted;

            received += compute_received_delta(last_, din.keep);
            received_upper_bound += DataWidth;

            // Incrementally computes last_index % DataWidth
            // (ensures that last_index < DataWidth by the time the loop finishes).
            // Note that if the loop exists early (because LengthPolicy is Checked,
            // and it hits the last flag),
            // then last_index will be wrong, but that doesn't matter because
            // all that will result in is an incorrect len_.
            // However, once we've hit last, the only way to read more is to reset,
            // which will 0 len_.
            last_index -= DataWidth;
        }

        // Shift the work buffer right to trim off the extra leading bytes,
        // and assign it to the correct size vector to truncate any higher bytes
        Shift leading_bytes = Shift(DataWidth) - Shift(len_);
        util::Bytes<MsgWidth> result = work >> (leading_bytes * util::BITS_PER_BYTE);

        // Update len to be the number of new bytes now stored in buf_
        len_ = DataWidth - last_index - 1;

        if (received > MsgWidth) {
            received = MsgWidth;
        }
        return LengthPolicy::template result<MsgWidth>(result, received);
    }

    template <typename LengthPolicy, int H, int... W>
    void read_raw(typename LengthPolicy::template Result<H>& out, typename LengthPolicy::template Result<W>&... rest) {
        HLS_PRAGMA(HLS inline);
        const int total = util::Sum<H, W...>::value;
        typename LengthPolicy::template Result<total> result = read_raw<LengthPolicy, total>();
        assign_sub_result<LengthPolicy, total, H, W...>(result, 0, out, rest...);
    }

private:
    void reset() {
        HLS_PRAGMA(HLS inline);
        buf_ = 0;
        len_ = 0;
        last_ = false;
    }

    template <typename LengthPolicy, int T>
    void assign_sub_result(const typename LengthPolicy::template Result<T>&, util::uint_t<T>) {
        HLS_PRAGMA(HLS inline);
    }

    template <typename LengthPolicy, int T, int H, int... W>
    void assign_sub_result(const typename LengthPolicy::template Result<T>& result, util::uint_t<T> start_index, typename LengthPolicy::template Result<H>& out, typename LengthPolicy::template Result<W>&... rest) {
        HLS_PRAGMA(HLS inline);
        assign_sub_result<LengthPolicy, T, W...>(result, start_index + H, rest...);
        out = LengthPolicy::template sub_result<H, T>(result, start_index);
    }

    template <typename LengthPolicy, int T>
    void assign_sub_result_typed(const typename LengthPolicy::template Result<T>&, util::uint_t<T>) {
        HLS_PRAGMA(HLS inline);
    }

    template <typename LengthPolicy, int T, typename H, typename... W>
    void assign_sub_result_typed(const typename LengthPolicy::template Result<T>& result, util::uint_t<T> start_index, typename LengthPolicy::template TypedResult<H>& out, typename LengthPolicy::template TypedResult<W>&... rest) {
        HLS_PRAGMA(HLS inline);
        static_assert(H::width % util::BITS_PER_BYTE == 0, "Type must be a integer number of bytes");
        const int num_bytes = H::width / util::BITS_PER_BYTE;

        // Note: it is critical that the recursive call occurs before the assignment!
        // If the assignment occurs before the call, it is possible the call _could_ change the value of out
        // This is because this function takes a lot of outputs by reference, and out could be (though in practice never will be)
        // duplicated later in the argument list. As such, the final value of out isn't clear; it depends on all the subsequent
        // recursive calls being invoked.
        // However, if you assign out after the call, the final value of out is explicit: it _must_ be the one assigned here.
        // As such, this ordering of the call and the assignment prevents the HLS tool from inferring a false dependency,
        // and allows it to do all the assignments in parallel.
        assign_sub_result_typed<LengthPolicy, T, W...>(result, start_index + num_bytes, rest...);
        out = LengthPolicy::template typed_result<H, num_bytes>(LengthPolicy::template sub_result<num_bytes, T>(result, start_index));
    }

    util::uint_t<DataWidth> compute_received_delta(bool last, ap_uint<DataWidth> keep) {
        HLS_PRAGMA(HLS inline);
        if (!last) {
            return DataWidth;
        } else {
            // Consider some examples (assuming DataWidth = 8):
            // keep = 11111111 (keep everything)
            // add a 0 (011111111), invert (100000000), reverse (000000001),
            // and count leading zeros -> 8
            // keep = 00000001 (keep only the first byte)
            // add a 0 (000000001), invert (111111110), reverse (011111111),
            // and count leading zeros -> 1
            // Note that keep = 00000000 is not valid --
            // last should have been set on the prior beat. But this function will
            // return 0, which is correct regardless.
            return (~ap_uint<DataWidth + 1>(keep)).reverse().countLeadingZeros();
        }
    }

    using Len = util::state_t<DataWidth>;

    // High len_ bytes contain leftover data
    util::Bytes<DataWidth> buf_;
    Len len_;

    // True if the data currently in buf_ was the end of an AXI packet
    bool last_;

    AxiStream<DataWidth>& stream_;
};
