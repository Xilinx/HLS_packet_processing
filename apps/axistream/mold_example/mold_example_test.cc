#include "catch.hpp"
#include "mold_example.hh"
#include "axi_stream_reader.hh"
#include "axi_stream_writer.hh"
#include "ethernet.hh"
#include "ip.hh"
#include "udp.hh"
#include "moldudp64.hh"
#include "itch.hh"

TEST_CASE("mold_example") {
    AxiStream<8> input_stream;
    hls::stream<int32_t> output_stream;
    AxiStreamWriter<8> writer(input_stream);

    ethernet::Header e;
    ip::v4::Header ip;
    udp::Header udp;
    moldudp64::Header mold;

    e.set<ethernet::EtherType>(ethernet::ETHER_TYPE::IPV4);
    e.set<ethernet::DestinationMac>(0xc0ffee);
    ip.set<ip::v4::Protocol>(ip::v4::PROTOCOL::UDP);
    ip.set<ip::v4::DestinationIP>(0xdeadbeef);
    udp.set<udp::DestinationPort>(0xf00d);
    mold.set<moldudp64::MessageCount>(2);
    writer.write(e, false);
    writer.write(ip, false);
    writer.write(udp, false);
    writer.write(mold, false);

    moldudp64::MessageHeader message_header;
    itch::Header itch_header;
    itch::StockDirectory stock_directory;
    itch::AddOrder add_order;
    itch_header.set<itch::MessageType>(itch::MESSAGE_TYPE::STOCK_DIRECTORY);
    writer.write(message_header, false);
    writer.write(itch_header, false);
    writer.write(stock_directory, false);
    itch_header.set<itch::MessageType>(itch::MESSAGE_TYPE::ADD_ORDER_NO_MPID_ATTRIBUTION);
    writer.write(message_header, false);
    writer.write(itch_header, false);
    writer.write(add_order, true);

    mold_example(input_stream, output_stream);

    CHECK(output_stream.read() == 1);
    CHECK(output_stream.read() == 2);
}
