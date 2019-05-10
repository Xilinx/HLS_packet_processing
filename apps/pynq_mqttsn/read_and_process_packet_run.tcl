# Copyright (c) 2016-2018, Xilinx, Inc.
# All rights reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

open_project hls_prj
set_top read_and_process_packet
add_files app.cpp -cflags "-I../common -I../../boost -DHLS_NO_XIL_FPO_LIB -DPLATFORM_pynqz1 -DRAW -Dread_and_process_packet_mqttsn_publish_EXPORTS --std=gnu++11 -Wno-unknown-attributes -Wno-multichar -fpic -D __SDSCC__ -m32 -I /proj/xbuilds/2017.1_sdx_0818_1/installs/lin64/SDx/2017.1/target/aarch32-linux/include -I../mqttsn_publish_qos_direct2 -D __SDSVHLS__ -D __SDSVHLS_SYNTHESIS__ -I ../mqttsn_publish_qos_direct2/read_and_process_packet -w"
add_files -tb "../common/eth_interface.cpp top.cpp main_simple.cpp" -cflags "-DRAW -I../common -I../../boost -I/group/xrlabs/tools/x86_64/include -DHLS_NO_XIL_FPO_LIB -DRAW -DPLATFORM_zc702 -Wno-unknown-attributes -Wno-multichar  -I /proj/xbuilds/2017.1_sdx_0612_1/installs/lin64/SDx/2017.1/target/aarch32-linux/include -I../mqttsn_publish_qos -D __SDSVHLS__ -D __SDSVHLS_SYNTHESIS__ -I ../mqttsn_publish_qos/zc702-sdsoc -w -std=gnu++11"
open_solution "solution" -reset
set_part { xc7z020clg400-1 }
# synthesis directives
create_clock -period 10.000000
set_clock_uncertainty 27.0%
config_rtl -reset_level low
source read_and_process_packet.tcl
# end synthesis directives
config_rtl -prefix a0_
csim_design -compiler clang
csynth_design
cosim_design -compiler clang -trace_level all
#export_design -ipname read_and_process_packet -acc

