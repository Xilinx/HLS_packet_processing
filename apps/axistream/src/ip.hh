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


#include "message.hh"
#include <cstdint>

namespace ip { namespace v4 {
    using namespace message;

    // Header: https://en.wikipedia.org/wiki/IPv4#Header
    // https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
    enum class PROTOCOL : uint8_t {
        TCP = 0x06,
        UDP = 0x11,
    };

    struct Version : lu<4> {};
    struct IHL : lu<4> {};
    struct DSCP : lu<6> {};
    struct ECN : lu<2> {};
    struct TotalLength : bu16 {};
    struct Identification : bu16 {};
    struct Flags : lu<3> {};
    struct FragmentOffset : lu<13> {};
    struct TimeToLive : u8 {};
    struct Protocol : Enum<PROTOCOL, Endianness::LITTLE> {};
    struct HeaderChecksum : lu16 {};
    struct SourceIP : lu32 {};
    struct DestinationIP : lu32 {};

    using Header = Struct<
        Version,
        IHL,
        DSCP,
        ECN,
        TotalLength,
        Identification,
        Flags,
        FragmentOffset,
        TimeToLive,
        Protocol,
        HeaderChecksum,
        SourceIP,
        DestinationIP>::type;
}}
