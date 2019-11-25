#pragma once

#include "hls_ap_axi_sdata.hh"
#include "hls_stream.hh"
#include "util.hh"

template <int B>
using Axi = ap_axiu<B * util::BITS_PER_BYTE, 1, 1, 1>;

template <int B>
using AxiStream = ::hls::stream<Axi<B>>;

