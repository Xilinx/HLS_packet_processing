#include <cstdint>
#include "mold_example.hh"
#include "axi_stream_reader.hh"
#include "ethernet.hh"
#include "ip.hh"
#include "udp.hh"
#include "moldudp64.hh"
#include "itch.hh"

void mold_example(AxiStream<8>& in_stream, hls::stream<int32_t>& out) {
    HLS_PRAGMA(HLS interface port=in_stream axis);
    HLS_PRAGMA(HLS interface port=out ap_fifo);
    HLS_PRAGMA(HLS interface ap_ctrl_none port=return);

    AxiStreamReader<8> reader(in_stream);
    ethernet::Header e;
    ip::v4::Header ip;
    udp::Header udp;
    moldudp64::Header mold;

    reader.read<LengthPolicy::Unchecked>(e, ip, udp, mold);
    if (e.get<ethernet::EtherType>() != ethernet::ETHER_TYPE::IPV4 ||
        e.get<ethernet::DestinationMac>() != 0xc0ffee ||
        ip.get<ip::v4::Protocol>() != ip::v4::PROTOCOL::UDP ||
        ip.get<ip::v4::DestinationIP>() != 0xdeadbeef ||
        udp.get<udp::DestinationPort>() != 0xf00d) {
        reader.drain();
        return;
    }

mold_message_loop:
    for(int i = 0; i < mold.get<moldudp64::MessageCount>(); i++) {
        moldudp64::MessageHeader message_header;
        itch::Header itch_header;
        reader.read<LengthPolicy::Unchecked>(message_header, itch_header);
        if(itch_header.get<itch::MessageType>() == itch::MESSAGE_TYPE::STOCK_DIRECTORY) {
            reader.read<LengthPolicy::Unchecked, itch::StockDirectory>();
            out.write(1);
        } else if (itch_header.get<itch::MessageType>() == itch::MESSAGE_TYPE::ADD_ORDER_NO_MPID_ATTRIBUTION) {
            reader.read<LengthPolicy::Unchecked, itch::AddOrder>();
            out.write(2);
        }
    }
}
