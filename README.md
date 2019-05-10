# HLS Packet Parsing

This repository contains a set of Vivado HLS libraries supporting networking applications.

Several applications of the library are provided:

- apps/arp: An ARP client implementation including ARP cache.
- apps/mold_remover_packet: A parser for MOLD/ITCH messages, using the "packet-oriented API".
- apps/mold_remover_stream: A parser for MOLD/ITCH messages, using the "stream-oriented API".
- apps/traffic_manager: A packet FIFO which preferentially drops low-priority packets.
- apps/pynq_mqttsn: An implementation of the UDP-based MQTTSN publish/subscribe protocol

The libraries have several goals in mind:
- Minimize multiplexing cost for fields with variable offset.
- Allow parameterized code to deal with different I/O width  (32-512 bits)
- Capture common header formats in library

Conceptually, the approach is inspired by the P4 packet processing language (https://p4.org/),
but is embedded in C++ and uses Vivado HLS to generate an FPGA implementation.  This can make verification
convient.  A number
of existing networking designs in Vivado HLS (e.g. https://github.com/Xilinx/HLx_Examples/tree/master/Acceleration/memcached,
https://github.com/fpgasystems/fpga-network-stack) use an RTL-like FSM-oriented coding style.  In constrast,
these libraries leverage more capabilities of Vivado HLS to automatically insert pipeline stages as
appropriate.  These libraries are implemented in C++11 using metaprogramming techniques from the `boost::mpl`
library.  This approach works with current versions of Vivado HLS (e.g. 2018.3), although it is not yet officially supported.

# Using the library

The library depends on boost 1.58.0.  If you've checked out the repository, you can check out and build headers for the corresponding version of boost using:

```
git submodule update --init --recursive
pushd boost
./bootstrap.sh
./b2 headers
popd
```

Fixed packet headers are represented using `boost::mpl::vector`, and individual fields of the header can be accessed.
```
namespace ipv4 {
    typedef newfield<ap_uint<16>, boost::mpl::string<'sprt'> > sport;
    typedef newfield<ap_uint<16>, boost::mpl::string<'dprt'> > dport;
    typedef newfield<ap_uint<16>, boost::mpl::string<'leng'> > length;
    typedef newfield<ap_uint<16>, boost::mpl::string<'csum'> > checksum; 

    using udp_header = fixed_header<boost::mpl::vector<sport, dport, length, checksum> >;
};

udp_header h;
h.set<sport>(5555);
int i = h.get<sport>();
```

## Packet-oriented coding
Two coding styles exist for serializing headers and packets over a stream of arbitrary (power of 2) width.
The first style is a **packet-oriented** coding style, where the structure of a packet of interest is
defined as a cascade of headers, followed by the payload, represented by the `Packet` class.  An entire
packet can be sent and received using the `serialize()` and `deserialize()` functions.
```
#define BYTESPERCYCLE 8
typedef ap_axiu<8*BYTESPERCYCLE,1,1,1> StreamType;

void ingress(hls::stream<StreamType> &input,    // input
             hls::stream<StreamType> &internal,    // output
             hls::stream<short> &input_length_stream1, // output
             hls::stream<short> &input_length_stream2, // output
             hls::stream<ap_uint<6> > &diffserv_stream // output) {
    
    short input_length;
    ap_uint<6> diffserv;

    Packet p;
    ipv4_hdr<Packet> ih(p);
    ethernet_hdr<ipv4_hdr<Packet> > eh(ih);
    eh.deserialize(input);
    diffserv = ih.diffserv.get() >> 2; // Drop the ECN field.
    input_length = eh.data_length();
    eh.serialize(internal);
```

Code like above generates a store-and-forward architecture in Vivado HLS.  Headers are stored in Flip-Flops
and can be cheaply accessed in parallel.  Payloads are stored in BRAM/URAM memory and can only be accessed
sequentially.  With Dataflow mode in Vivado HLS, payload storage can be automatically double-buffered, enabling
simultaneous sending and receiving of packets at close to line rate.  Note, however that the end-to-end
latency of such a design will typically be more than one packet time.  In addition, the library assumes that
input and output packets are always properly framed using TLAST in an AXI stream.  This makes it impossible to
generate an improperly framed output, since the framing is implemented in the library.  Currently the library
has no good way of recovering from improperly framed inputs.

Although the above example is quite simple, more complex operations on Packets are possible.  In particular,
headers can be conceptually added and removed.  Also, Packets can be serialized and deserialized from arrays of bytes.
```
// Removing a header
    Packet p;
    header<Packet, 40> ih(p);
    ethernet_hdr<header<Packet, 40> > eh(ih);
    eh.deserialize(buf, len);
    ih.serialize(output, len);
```
```
// Adding a header
    Packet p;
    header<Packet, 40> ih(p);
    ethernet_hdr<header<Packet, 40> > eh(ih);
    ih.deserialize(buf, len);
    eh.serialize(output, len);
```
Additionally, deserialized packets can be reparsed in other formats. Such a reparsing shares the same underlying storage as the original packet, but enables parsing of packets with different header combinations.  The return value is a 'header-like' object which supports the same indexing operations.
```
    Packet p;
    header<Packet, 40> ih(p);
    ethernet_hdr<header<Packet, 40> > eh(ih);
    eh.deserialize(buf, len);
 
    MACAddressT destinationMAC = eh.get<destinationMAC>();
    ap_uint<16> dmp_macType = eh.get<etherType>();
 
   if (dmp_macType == ethernet::ethernet_etherType::ARP) {
        stats.arps_received++;
        auto ah = parse_arp_hdr(ih);
        do_arp(ah)
    } else {
        auto h = ipv4::parse_ipv4_hdr(ih);
        if(h.get<ipv4::protocol>() == ipv4::ipv4_protocol::UDP) {
            auto uh = ipv4::parse_udp_hdr(h.p);
            ...
```
Lastly, we can 'skip' an arbitrary number of bytes in a packet.  Again, this returns a header-like object.   Note, however, that if the offset is not a compile-time constant, then indexing into the resulting object requires reading at a variable address.  This almost inevitably requires a circuit with wide multiplexors.  If significant reading of the fields of a packet after a variable offset are required, then it is usually more hardware efficient to serialize and deserialize the packet, enabling more efficient access.
```
Packet p;
mold_hdr<Packet> mh(p);

int offset = 0;

// Iterate over each mold message, starting at offset 0
// after the mold header.
for(int i = 0; i < mh.get<messageCount>(); i++) {

   // Extract a mold message from the 'packet' portion
   auto next_message = skip(p, offset);
   auto mold_message = parse_mold_message_hdr(next_message);

   // Look at the mold message to figure out where the next one starts.
   // the length doesn't include the size of the length field itself.
   offset += mold_message.get<messageLength>() + 2;

   // Extract the itch message and do something with it.
   auto itch_message = parse_itch_hdr(mold_message.p);
```

## Stream-oriented coding style
An alternative coding style reads headers one at a time from the input stream.  Normally, this is somewhat awkward to implement, since the core `hls::stream` object is intended to be accessed only a data beat at a time, while headers are rarely the same size as a data beat and rarely aligned.   This coding style is implemented by **reader** and **writer** objects, which encapsulate an `hls::stream` object and provide the additional ability to read or write arbitrary headers of arbitrary size, along with a small amount of storage.
```
void ingress_buffer_writer(hls::stream<StreamType> &in) {
    auto reader = make_reader(in);
    ipv4::header ih;
    ipv4::udp_header uh;
    udt::data_header dh;
    reader.get(ih);
    reader.get(uh);
    reader.get(dh);

    bool isValidUDT =  check_headers(ih,uh,dh);
    if(isValidUDT) {
        // Copy the packet to the external buffer.
    write_loop:
        bool done = false;
        int inCount = 0;
        while(!done) {
#pragma HLS PIPELINE
            StreamType tmp;
            reader.read(tmp);
            if(inCount < MTU+1) {
                buffer_storage[inCount/BYTESPERCYCLE][buffer_id] = tmp.data;
                inCount += keptbytes(tmp.keep);
            }
            if(tmp.last)
                done = true;
        }
    } else {
        reader.read_rest();
    }
}
```
This coding style naturally leads to cut-through packet processing, where the latency from input to output is approximately one packet.   It also has the advantage that infinite streams of data (i.e. a reconstructed TCP stream) can be parsed, making it better able to handle higher-level (L4-L7) protocols.  The main disadvantage is that it is possible to create incorrectly framed streams (missing the TLAST at end of packet) and that generally speaking, the programmer must be somewhat aware of the state stored in readers and writers.

As with almost any packet-processing model, headers can be added and removed by simply writing code that sends and receives the correct headers and payload, since there is no concept of a 'packet object'.
```
        // Remove header
        auto reader = make_reader(dataIn);
        auto writer = make_writer(dataOut);
        ethernet::header x = reader.get<ethernet::header>();
        writer.put_rest(reader);
```
```
        // Add header
        auto reader = make_reader(dataIn);
        auto writer = make_writer(dataOut);
        ethernet::header x;
         for(int i = 0; i < LENGTH; i++) {
            x.set<1>(i, i-(LENGTH));
        }
        writer.put(x);
        writer.put_rest(reader);
```

# Other libraries
In addition to packet parsing, networking application often require a number of other data structures to construct an overall design.  The library contains several support libraries with useful data structures.

### hls::cam
A Parallel-match CAM.
Uses O(N) LUTs/FFs. II=1 lookup, insert/delete. 

### hls::algorithmic_cam
A Cuckoo-Hashing CAM efficient for large N.
Uses O(N) BRAM bits. ~II=1 lookup, insert/delete (assuming insert and delete are rare operations)

### hls::allocator
A simple freelist based allocator with II=1 allocate/deallocate

### hls::message_buffer implements 
A bag-like data structure with II=1 insert/pick


## License

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
