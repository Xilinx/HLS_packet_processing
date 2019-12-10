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

namespace itch {
    using namespace message;

    enum class MESSAGE_TYPE : uint8_t {
        SYSTEM_EVENT = 'S',
        STOCK_DIRECTORY = 'R',
        ADD_ORDER_NO_MPID_ATTRIBUTION = 'A',
    };

    struct MessageType : Enum<MESSAGE_TYPE, Endianness::BIG> {};
    struct StockLocate : bu16 {};
    struct TrackingNumber : bu16 {};
    struct Timestamp : bu<48> {};
    using Header = Struct<MessageType, StockLocate, TrackingNumber, Timestamp>::type;

    // 4.1 System Event Message
    enum class SYSTEM_EVENT_CODE : uint8_t {
        START_OF_MESSAGES = 'O',
        START_OF_SYSTEM_HOURS = 'S',
        START_OF_MARKET_HOURS = 'Q',
        END_OF_MARKET_HOURS = 'M',
        END_OF_SYSTEM_HOURS = 'E',
        END_OF_MESSAGES = 'C',
    };
    struct EventCode : Enum<SYSTEM_EVENT_CODE, Endianness::BIG>{};

    using SystemEventMessage = Struct<EventCode>::type;

    // 4.2.1 Stock Directory
    struct Stock : Array<u8, 8> {};
    struct MarketCategory : u8 {};
    struct FinancialStatusIndicator : u8 {};
    struct RoundLotSize : bu32 {};
    struct RoundLotsOnly : u8 {};
    struct IssueClassification : u8 {};
    struct IssueSubType : bu16 {};
    struct Authenticity : u8 {};
    struct ShortSaleThresholdIndicator : u8 {};
    struct IPOFlag : u8 {};
    struct LULDReferencePriceTier : u8 {};
    struct ETPFlag : u8 {};
    struct ETPLeverageFactor : bu32 {};
    struct InverseIndicator : u8 {};

    using StockDirectory = Struct<
        Stock,
        MarketCategory,
        FinancialStatusIndicator,
        RoundLotSize,
        RoundLotsOnly,
        IssueClassification,
        IssueSubType,
        Authenticity,
        ShortSaleThresholdIndicator,
        IPOFlag,
        LULDReferencePriceTier,
        ETPFlag,
        ETPLeverageFactor,
        InverseIndicator>::type;

    // 4.3.1 Add Order - No MPID Attribution
    struct OrderReferenceNumber : bu64 {};
    struct BuySell : u8 {};
    struct Shares : bu32 {};
    // Stock alredy declared above
    // struct Stock : Array<u8, 8> {};
    struct Price : bu32 {};

    using AddOrder = Struct<
        OrderReferenceNumber,
        BuySell,
        Shares,
        Stock,
        Price>::type;
}
