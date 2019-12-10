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

namespace moldudp64 {
    using namespace message;

    // https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf
    struct Session : Array<u8, 10> {};
    struct SequenceNumber : bu64 {};
    struct MessageCount : bu16 {};

    using Header = Struct<Session, SequenceNumber, MessageCount>::type;

    struct MessageLength : bu16 {};
    using MessageHeader = Struct<MessageLength>::type;
}
