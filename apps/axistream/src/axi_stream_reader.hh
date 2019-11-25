#pragma once

#include <type_traits>
#include "hls_macros.hh"
#include "util.hh"
#include "axi_stream_common.hh"
#include "message.hh"

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
        while (!last_) {
            HLS_PRAGMA(HLS loop_tripcount min=0);
            last_ = stream_.read().last;
        }
        reset();
    }

    /*
     * Reads a message::Struct from the stream
     */
    template <typename T>
    T read() {
        HLS_PRAGMA(HLS inline);
        // Messages may be a non-integer number of bytes in length, since in general they can
        // have fields which are not integer numbers of bytes.
        static_assert(T::width % util::BITS_PER_BYTE == 0, "Type must be a integer number of bytes");
        return T::from_raw(read_raw<T::width / util::BITS_PER_BYTE>());
    }

    /*
     * Given some message T which starts with header H, reads all non-header bytes
     * of T from the stream, then concatenates the new bytes with the given header
     * to return a complate T.
     *
     * This overload works for types T which are strictly larger than the headers.
     */
    template <typename T, typename H>
    typename std::enable_if<(T::width > H::width), T>::type read_after(const H& header) {
        HLS_PRAGMA(HLS inline);
        const int to_read = T::width - H::width;
        static_assert(to_read % util::BITS_PER_BYTE == 0, "Remainder must be an integer number of bytes");
        return T::from_raw((read_raw<to_read / util::BITS_PER_BYTE>(), H::to_raw(header)));
    }

    /*
     * Given some message T which starts with header H, reads all non-header bytes
     * of T from the stream, then concatenates the new bytes with the given header
     * to return a complate T.
     *
     * This overload works for types T which are the same size as the header - in other words
     * the type consists of only a header.
     */
    template <typename T, typename H>
    typename std::enable_if<T::width == H::width, T>::type read_after(const H& header) {
        HLS_PRAGMA(HLS inline);
        return T::from_raw(H::to_raw(header));
    }

    /*
     * Reads the specified number of byes from the stream.
     */
    template <int MsgWidth>
    util::Bytes<MsgWidth> read_raw() {
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
        // Maximum number of new bytes we might receive minus 1
        // This is the maximal value received may take as of the end of the second to last
        // loop iteration, and as such the maximal value received needs to ever hold
        // (it doesn't matter if it overflows during the last loop iteration, because it will
        // never be read again).
        const int max_received = DataWidth * max_beats - 1;
        // Type which can store the number of bytes to shift the final result by
        // At most, all the low DataWidth bytes need to be shifted
        typedef ap_uint<util::unsigned_bit_width<DataWidth * util::BITS_PER_BYTE>::value> Shift;

        // Buf will be zero extended since work is MsgWidth longer than buf_
        ap_uint<work_size> work = buf_;
        // Represents the number of bytes we have so far
        ap_uint<util::unsigned_bit_width<max_received>::value> received = len_;
        // Represents the index in work where the last byte of the message will be written
        ap_uint<util::unsigned_bit_width<work_size - 1>::value> last_index = DataWidth - len_ + MsgWidth - 1;

        for (int i = 0; i < max_beats; i++) {
            HLS_PRAGMA(HLS unroll);
            // Not including this if statement in the loop guard allows Vivado to generate better
            // code because then the loop has a fixed number of iterations.
            if (received < MsgWidth) {
                Axi<DataWidth> din = stream_.read();
                buf_ = din.data;
                last_ = din.last;

                // This assignment will truncate the most significant bits
                ap_uint<work_size> shifted = ap_uint<work_size>(din.data) << (util::BITS_PER_BYTE * (i + 1) * DataWidth);
                work |= shifted;
                received += DataWidth;

                // Incrementally computes last_index % DataWidth
                // (ensures that last_index < DataWidth by the time the loop finishes).
                last_index -= DataWidth;
            }
        }

        // Shift the work buffer right to trim off the extra leading bytes,
        // and assign it to the correct size vector to truncate any higher bytes
        Shift leading_bytes = Shift(DataWidth) - Shift(len_);
        util::Bytes<MsgWidth> result = work >> (leading_bytes * util::BITS_PER_BYTE);

        // Update len to be the number of new bytes now stored in buf_
        len_ = DataWidth - last_index - 1;

        return result;
    }

private:
    void reset() {
        HLS_PRAGMA(HLS inline);
        buf_ = 0;
        len_ = 0;
        last_ = false;
    }

    typedef ap_uint<util::unsigned_bit_width<DataWidth - 1>::value> Len;

    // High len_ bytes contain leftover data
    util::Bytes<DataWidth> buf_;
    Len len_;

    // True if the data currently in buf_ was the end of an AXI packet
    bool last_;

    AxiStream<DataWidth>& stream_;
};

