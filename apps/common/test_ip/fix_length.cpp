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

#include "ip_handler.hpp"
#include "ip.hpp"
#include "test_utils.h"

using namespace hls;

int main(int argc, char* argv[]) {
    stream<axiWord> inFIFO("inFIFO");
	stream<axiWord> outFIFO("outFIFO");
    readStreamFromFile(inFIFO, argv[1]);

    while(!inFIFO.empty()) {
        Packet p;
        ipv4_hdr<Packet> ih(p);
        ethernet_hdr<ipv4_hdr<Packet> > h(ih);
        h.deserialize(inFIFO);
        ap_uint<16> length = ih.get_length(); // Get the length field of the header.
        if(length != ih.data_length()) {
            std::cout << "Length Incorrect!\n";
            ih.extend(length); // Force the payload length to be correct.
        }
        h.serialize(outFIFO);
    }

    compareStreamWithFile(outFIFO, argv[1], argv[2]);
}
