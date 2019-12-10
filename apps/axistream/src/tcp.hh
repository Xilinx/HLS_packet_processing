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
#include "message.hh"

namespace tcp {
    using namespace message;

    // Header: https://en.wikipedia.org/wiki/Transmission_Control_Protocol#TCP_segment_structure
    struct SourcePort : bu16 {};
    struct DestinationPort : bu16 {};
    struct SeqNumber : bu32 {};
    struct AckNumber : bu32 {};
    struct DataOffset : lu<4> {};
    struct Reserved : lu<3> {};
    struct NS : lu<1> {};
    struct CWR : lu<1> {};
    struct ECE : lu<1> {};
    struct URG : lu<1> {};
    struct ACK : lu<1> {};
    struct PSH : lu<1> {};
    struct RST : lu<1> {};
    struct SYN : lu<1> {};
    struct FIN : lu<1> {};
    struct WindowSize : bu16 {};
    struct Checksum : bu16 {};
    struct UrgentPointer : bu16 {};

    using Header = Struct<
        SourcePort,
        DestinationPort,
        SeqNumber,
        AckNumber,
        DataOffset,
        Reserved,
        NS,
        CWR,
        ECE,
        URG,
        ACK,
        PSH,
        RST,
        SYN,
        FIN,
        WindowSize,
        Checksum,
        UrgentPointer>::type;
}
