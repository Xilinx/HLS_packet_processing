# Axistream

## Contents
* `src`: the library itself
* `test`: unit tests
* `tl`: a simple top-level function for HLS synthesis
* `mold_example`: an example for parsing NASDAQ ITCH marketdata events over moldudp64. (Based off of: https://github.com/Xilinx/HLS_packet_processing/tree/master/apps/mold_remover_stream)

## Building
The tests require [Catch2](https://github.com/catchorg/Catch2).
Download `catch.hpp`, and then set the `CATCH_DIR` variable in the `Makefile`
accordingly.

Configure the following 2 lines the in Makefile:
```
CATCH_DIR = ../catch
HLS_INCLUDE_DIR = /u/tsqahfus/local/tools/xilinx/Vivado/2018.3/include/
```

Then run `make`.
