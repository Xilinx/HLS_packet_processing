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

namespace udp {
    using namespace message;

    // Header: https://en.wikipedia.org/wiki/User_Datagram_Protocol#Packet_structure
    struct SourcePort : bu16 {};
    struct DestinationPort : bu16 {};
    struct Length : bu16 {};
    struct Checksum : bu16 {};

    using Header = Struct<
        SourcePort,
        DestinationPort,
        Length,
        Checksum>::type;
}
