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

namespace ouch {
    using namespace message;

    // http://www.nasdaqtrader.com/content/technicalsupport/specifications/tradingproducts/ouch4.2.pdf
    // Header fields common to every ouch message
    enum class MESSAGE_TYPE : uint8_t {
        EXECUTED = 'E',
        INVALID = 'X'
    };
    struct MessageType : Enum<MESSAGE_TYPE, Endianness::BIG> {};
    struct Timestamp : bu64 {};
    using Header = Struct<MessageType, Timestamp>::type;


    struct OrderToken : Array<u8, 14> {};
    struct ExecutedShares : bu32 {};
    struct ExecutionPrice : bu32 {};
    struct LiquidityFlag : u8 {};
    struct MatchNumber : bu64 {};
    using OrderExecuted = Struct<OrderToken, ExecutedShares, ExecutionPrice, LiquidityFlag, MatchNumber>::type;
}
