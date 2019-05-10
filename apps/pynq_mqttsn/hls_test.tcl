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

open_project process_packet
set_top process_packet
add_files app.cpp -cflags "--std=c++0x -I../common -I../../boost"
add_files -tb hls_test.cpp -cflags "--std=c++0x -I../common -I../../boost"
open_solution "solution" -reset
set_part { xc7z020clg400-1 }
# synthesis directives
config_compile -name_max_length 4000 -ifconvert_depth 1
create_clock -period 10.000000
config_rtl -reset_level low
set_directive_data_pack process_packet inBuf
set_directive_interface -mode m_axi -offset direct "process_packet" inBuf -bundle=inBuf -depth 4096
set_directive_data_pack process_packet outBuf
set_directive_interface -mode m_axi -offset direct "process_packet" outBuf -bundle=outBuf -depth 4096
set_directive_latency -min 1 process_packet

# end synthesis directives
#csim_design -compiler clang
csynth_design
#cosim_design -bc
cosim_design -trace_level all
