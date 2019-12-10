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
#include <cstdint>

namespace ethernet {
    using namespace message;

    enum class ETHER_TYPE : uint16_t {
        IPV4 = 0x0800
    };

    // Header: https://en.wikipedia.org/wiki/IEEE_802.1Q
    struct DestinationMac : lu<48> {};
    struct SourceMac : lu<48> {};
    // 802.1Q VLAN header
    struct TPID : bu16 {};
    struct PCP : lu<3> {};
    struct DEI : lu<1> {};
    struct VID : lu<12> {};
    // End VLAN header
    struct EtherType : Enum<ETHER_TYPE, Endianness::BIG> {};

    using Header = Struct<
        DestinationMac,
        SourceMac,
        TPID,
        PCP,
        DEI,
        VID,
        EtherType>::type;
}
