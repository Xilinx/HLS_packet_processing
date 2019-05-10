/*
Copyright (c) 2016-2018, Xilinx, Inc.
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <stdio.h>

#include "app.h"
#include "ip.hpp"
//#include "allocator.h"
//#include "eth_interface.h"
#include "hls_math.h"
#include <tuple>


bool TEST_generate_output = true;

/** This process supports several independent operations:
* "ingress": (input_length, diffserv) -> buffer_id
* "egress_free": completed ->
* "egress_allocate": -> (output, outputLength)
*
* When an "ingress" action occurs, the length is stored internally.
* This length will be returned with the next egress_allocate action
* with output == buffer_id
*/
void priority_queue_manager(hls::stream<short> &input_length_stream, // input
                            hls::stream<ap_uint<6> > &diffserv_stream, // input
                            hls::stream<bufferIDT> &buffer_id_stream, // output
                            hls::stream<bufferIDT> &completed, // input
                            hls::stream<bufferIDT> &output,    // output
                            hls::stream<short> &outputLength    // output
                            ) {
#pragma HLS interface port=return ap_ctrl_none
#pragma HLS interface port=input_length_stream axis
#pragma HLS interface port=diffserv_stream axis
#pragma HLS interface port=buffer_id_stream axis
#pragma HLS interface port=completed axis
#pragma HLS interface port=output axis
#pragma HLS interface port=outputLength axis

#pragma HLS stream variable=output depth=32
#pragma HLS stream variable=outputLength depth=32
#pragma HLS inline all recursive
#pragma HLS pipeline II=4
    static orderT last_written = 0; // Record the order that packets arrive.
    static orderT last_read = -1; // Record the order that packets leave.

    // Together these implement a priority-queue structure.
    static std::pair<orderT, bufferIDT> id_by_category[CATEGORYCOUNT][BUFFERCOUNT];
    static short length[BUFFERCOUNT] = {};
    static indexT read_index_by_category[CATEGORYCOUNT] = {};
    static indexT write_index_by_category[CATEGORYCOUNT] = {};
#pragma HLS array_partition variable=read_index_by_category complete
#pragma HLS array_partition variable=write_index_by_category complete
#pragma HLS array_partition variable=id_by_category complete dim=1

    // The category with the oldest packet.  This is the category
    // that we can read a packet from.
    int read_category = -1;
    //indexT read_index;
    // The id of the buffer for the oldest packet.
    bufferIDT read_id;
    // The length of the oldest packet.
    short read_length = 0;

    // Identify which categories are empty.
    bool empty[CATEGORYCOUNT];
    for(int i = 0; i < CATEGORYCOUNT; i++) {
#pragma HLS unroll
        empty[i] = read_index_by_category[i] == write_index_by_category[i];
    }

#ifndef __SYNTHESIS__
    std::cout << "empty@";
    for(int i = 0; i < CATEGORYCOUNT; i++) {
        std::cout << " " << (empty[i] ? "Y":"N");
    }
    std::cout << "\n";
#endif

    // Find the category with the oldest packet.
    // Set read_category, read_id, and read_length.
    int youngest_age = 0xFFFFFFF;
    int read_order = 0;
    for(int i = 0; i < CATEGORYCOUNT; i++) {
#pragma HLS unroll
        if(!empty[i]) {
            orderT order;
            int id;
            std::tie(order, id) = id_by_category[i][read_index_by_category[i]];
            orderT age = order - last_read; // explicit truncation so we can do a modulo subtraction.
            if(age < youngest_age) {
                youngest_age = age;
                read_category = i;
                read_id = id;
                read_order = order;
            }
        }
    }
    read_length = length[read_id];

    bool ingress_valid = !input_length_stream.empty();
    bool egress_valid = !output.full() && TEST_generate_output;
    //bool egress_valid = TEST_generate_output;

    bufferIDT write_id = -1;
    int drop_category = -1;
    int drop_index = 0;
    // This keeps track of which buffers have been put into circulation.
    static int allocated_buffer = 0;
    if(ingress_valid && !completed.empty()) {
        write_id = completed.read();
#ifndef __SYNTHESIS__
        std::cout << "Using old ID " << write_id << "\n";
#endif
    } else if(ingress_valid && allocated_buffer < BUFFERCOUNT) {
        // Allocate a buffer ID to store the packet in
        write_id = allocated_buffer++;
    } else if(ingress_valid) { // Failed to allocate a buffer.
#ifndef __SYNTHESIS__
        std::cout << "Attempting to Reallocate..\n";
#endif
        // Drop a lowest priority packet and use its buffer id.
        // (In this case, we choose to drop the oldest packet in the category,
        // because that's relatively simple to implement)
        int count = 0;
        // Find the category with the lowest priority
        for(int i = 0; i < CATEGORYCOUNT; i++) {
#pragma HLS unroll
            if(!empty[i]) {
                drop_category = i;
                drop_index = read_index_by_category[i];
                break;
            }
        }
        if(drop_category >= 0) {
            orderT order;
            std::tie(order, write_id) =
                id_by_category[drop_category][drop_index];
#ifndef __SYNTHESIS__
            std::cout << "Reallocating ID " << write_id << "\n";
#endif
        } else {
            assert("No buffers to allocate and none to recover" && false);
        }
    }

    short input_length = 0;
    ap_uint<6> diffserv = 0;
    if(ingress_valid) {
        input_length_stream >> input_length;
        diffserv_stream >> diffserv;
    }

    // Unclear exactly what the requirements for this function is, but as a guess
    // assume there are 8 categories of service corresponding to the upper 3 bits.
    // We have one queue per category.
    ap_uint<BitWidth<CATEGORYCOUNT>::Value> category = diffserv >> 3;

    if(ingress_valid) {
        id_by_category[category][write_index_by_category[category]++] = std::make_pair(last_written++, write_id);
#ifndef __SYNTHESIS__
        std::cout << "Writing ID " << write_id << " length=" << input_length << " from Category " << category << "\n";
#endif
        assert(write_index_by_category[category] < BUFFERCOUNT);
        assert(write_index_by_category[category] != read_index_by_category[category]); // There should be enough size in id_by_category that they can never fill up.
    }
    length[write_id] = input_length;

    // Egress path.  Note that the above two cases are almost always independent, so
    // that they can be scheduled in parallel.  However there is an occasional hazard
    // if we have an ingress packet and an egress packet and we've run out of buffers
    // to allocate and the buffer that we want to reuse is also the one we want to send.
    // In this case we reallocate the buffer for the ingress path and discard the old
    // buffer instead of forwarding it.
    // Additionally, we don't send a packet if the output queue is full.
    if(!egress_valid || ingress_valid && (drop_category == read_category)) {
        read_category = -1;
    }

    // Handle updating the read index;
    for(int i = 0; i < CATEGORYCOUNT; i++) {
#pragma HLS unroll
        if(i == read_category || i == drop_category) {
            read_index_by_category[i]++;
        }
    }
    if(read_category >= 0) {
#ifndef __SYNTHESIS__
        std::cout << "Reading ID " << read_id << " length=" << read_length << " from Category " << read_category << "\n";
#endif
        last_read = read_order;
        output << read_id;
        outputLength << read_length;
    }
    if(ingress_valid) {
        buffer_id_stream << write_id;
    }
#ifndef __SYNTHESIS__
    // std::cout << "Read@";
    // for(int i = 0; i < CATEGORYCOUNT; i++) {
    //     std::cout << " " << read_index_by_category[i];
    // }
    // std::cout << "\n";
    // std::cout << "Write@";
    // for(int i = 0; i < CATEGORYCOUNT; i++) {
    //     std::cout << " " << write_index_by_category[i];
    // }
    // std::cout << "\n";
    for(int i = 0; i < CATEGORYCOUNT; i++) {
        std::cout << "Category " << i << " (" << read_index_by_category[i] << " " << write_index_by_category[i] << ")";
        for(int j = read_index_by_category[i]; j < write_index_by_category[i]; j++) {
            orderT order;
            bufferIDT id;
            std::tie(order, id) = id_by_category[i][j];
            std::cout << " " << order << "@" << id;
        }
        std::cout << "\n";
    }
#endif
}

void ingress(hls::stream<StreamType> &input,    // input
             hls::stream<StreamType> &internal,    // output
             hls::stream<short> &input_length_stream1, // output
             hls::stream<short> &input_length_stream2, // output
             hls::stream<ap_uint<6> > &diffserv_stream // output
             ) {
#pragma HLS interface port=return ap_ctrl_none
#pragma HLS interface port=input axis
#pragma HLS interface port=internal axis
#pragma HLS interface port=input_length_stream1 axis
#pragma HLS interface port=input_length_stream2 axis
#pragma HLS interface port=diffserv_stream axis
    
    short input_length;
    ap_uint<6> diffserv;

    // Define the structure of the packets we're interested in.  Generally speaking we
    // have parallel access to the 'header' portion (the values are stored in registers),
    // and sequential access to the 'packet' portion (the values are stored in BRAM).
    Packet p;
    ipv4_hdr<Packet> ih(p);
    ethernet_hdr<ipv4_hdr<Packet> > eh(ih);
    eh.deserialize(input);
    diffserv = ih.diffserv.get() >> 2; // Drop the ECN field.
    input_length = eh.data_length();
    eh.serialize(internal);
    input_length_stream1 << input_length;
    input_length_stream2 << input_length;
    diffserv_stream << diffserv;
}

void ingress_new(hls::stream<StreamType> &input,    // input
             hls::stream<StreamType> &internal,    // output
             hls::stream<short> &input_length_stream1, // output
             hls::stream<short> &input_length_stream2, // output
             hls::stream<ap_uint<6> > &diffserv_stream // output
             ) {
#pragma HLS interface port=return ap_ctrl_none
#pragma HLS interface port=input axis
#pragma HLS interface port=internal axis
#pragma HLS interface port=input_length_stream1 axis
#pragma HLS interface port=input_length_stream2 axis
#pragma HLS interface port=diffserv_stream axis

    short input_length;
    ap_uint<6> diffserv;

    // Define the structure of the packets we're interested in.  Generally speaking we
    // have parallel access to the 'header' portion (the values are stored in registers),
    // and sequential access to the 'packet' portion (the values are stored in BRAM).
    auto reader = make_reader(input);
    auto writer = make_writer(internal);
    ethernet::header eh;
    reader.get(eh);
    ipv4::header ih;
    reader.get(ih);

    diffserv = ih.get<ipv4::diffserv>() >> 2; // Drop the ECN field.
    input_length = eh.data_length();
    input_length_stream1 << input_length;
    input_length_stream2 << input_length;
    diffserv_stream << diffserv;

    writer.put(eh);
    writer.put(ih);
    writer.put_rest(reader);
}
void ingress_writer(hls::stream<StreamType> &internal,    // input
                    hls::stream<short> &input_length_stream, // input
                    hls::stream<bufferIDT> &buffer_id_stream, // input
             ap_uint<8*BYTESPERCYCLE> buffer_storage[2048/BYTESPERCYCLE][BUFFERCOUNT] // written
             ) {
#pragma HLS interface port=return ap_ctrl_none
#pragma HLS interface port=internal axis
#pragma HLS interface port=input_length_stream axis
#pragma HLS interface port=buffer_id_stream axis
#pragma HLS interface port=buffer_storage bram
    bufferIDT buffer_id;
    buffer_id_stream >> buffer_id;
    short input_length;
    input_length_stream >> input_length;
    // Copy the packet to the external buffer.
    StreamType t;
 write_loop:
    for(int i = 0; i < input_length/BYTESPERCYCLE; i++) {
#pragma HLS pipeline II=1
        internal >> t;
        buffer_storage[i][buffer_id] = t.data;
    }
}

void egress(hls::stream<bufferIDT> &buffer_id_stream, // input
            hls::stream<short> &length_stream,    // input
            ap_uint<8*BYTESPERCYCLE> buffer_storage[2048/BYTESPERCYCLE][BUFFERCOUNT], // read
            hls::stream<bufferIDT> &completed, // output
            hls::stream<StreamType> &output   // output
            ) {
#pragma HLS interface port=return ap_ctrl_none
#pragma HLS interface port=buffer_id_stream axis
#pragma HLS interface port=length_stream axis
#pragma HLS interface port=completed axis
#pragma HLS interface port=output axis
#pragma HLS interface port=buffer_storage bram
    
    bufferIDT buffer_id;
    buffer_id_stream >> buffer_id;
    short length;
    length_stream >> length;
    // Copy the packet to the external buffer.
    StreamType t;
 write_loop:
    for(int i = 0; i < length/BYTESPERCYCLE; i++) {
#pragma HLS pipeline II=1
        t.data = buffer_storage[i][buffer_id];
        t.keep = -1; // FIXME: generate_keep
        output << t;
    }
    completed << buffer_id;
}

void process_packet(hls::stream<StreamType> &input,    // input
                    hls::stream<bufferIDT> &completed, // input
                    hls::stream<bufferIDT> &output,    // output
                    hls::stream<short> &outputLength,    // output
                    //Packet buffer_storage[BUFFERCOUNT] // Each buffer is 2Kbytes, assuming 1500 byte MTU.
                    ap_uint<8*BYTESPERCYCLE> buffer_storage[2048/BYTESPERCYCLE][BUFFERCOUNT] // Each buffer is 2Kbytes, assuming 1500 byte MTU.
                    ) {
    #pragma HLS dataflow

    hls::stream<StreamType> internal("internal");
#pragma HLS stream variable=internal depth=2048
    hls::stream<bufferIDT> Ioutput("Ioutput");
    hls::stream<short> IoutputLength("IoutputLength");
    hls::stream<short> input_length_stream1("input_length_stream1"); // input
    hls::stream<short> input_length_stream2("input_length_stream2"); // input
    hls::stream<ap_uint<6> > diffserv_stream("diffserv_stream"); // input
    hls::stream<bufferIDT> buffer_id_stream("buffer_id_stream"); // input
    ingress(input, internal, input_length_stream1, input_length_stream2, diffserv_stream);

    priority_queue_manager(input_length_stream2, diffserv_stream, buffer_id_stream,
                           completed, Ioutput, IoutputLength);

    ingress_writer(internal, input_length_stream1, buffer_id_stream, buffer_storage);



    // We get better QOR if all of the processes with
    // outputs end at the same time.
    if(!Ioutput.empty()) {
        bufferIDT read_id;
        short read_length;
        Ioutput >> read_id;
        IoutputLength >> read_length;
        output << read_id;
        outputLength << read_length;
    }
}
