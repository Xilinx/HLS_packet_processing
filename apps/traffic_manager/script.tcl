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

#open_project process_packet
#set_top process_packet

proc do_run {} {
add_files app.cpp -cflags "-I../common -I../../boost -DHLS_NO_XIL_FPO_LIB -DPLATFORM_zc702 --std=gnu++11 -Wno-unknown-attributes -Wno-multichar -D __SDSCC__ -w"
add_files -tb "main.cpp" -cflags "-I../common -I../../boost -DPLATFORM_zc702 -std=gnu++11 -Wno-unknown-attributes -Wno-multichar -w "
open_solution "solution" -reset
#set_part { xc7z020clg484-1 }
set_part "xczu9eg-ffvb1156-2-i"
# synthesis directives
create_clock -period 5.000000
#set_clock_uncertainty 27.0%
config_rtl -reset_level low
config_dataflow -default_channel fifo -fifo_depth 32
config_compile -name_max_length 10000
#csim_design -compiler clang
csynth_design
#cosim_design -compiler clang
export_design -evaluate verilog -flow impl

}

#open_project ingress
#set_top ingress
#do_run

open_project ingress_new
set_top ingress_new
do_run

open_project ingress_writer
set_top ingress_writer
do_run

open_project egress
set_top egress
do_run

open_project priority_queue_manager
set_top priority_queue_manager
do_run


