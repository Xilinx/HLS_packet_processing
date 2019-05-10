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

open_project test_allocator
set_top top
add_files test_allocator.cpp -cflags "--std=c++11 -Iboost_1_60_0 -Wall -Wno-unknown-attributes -Wno-unused-variable -Wno-pragmas"
add_files -tb test_allocator.cpp -cflags "--std=c++11 -Iboost_1_60_0 -Wall -Wno-unknown-attributes -Wno-unused-variable -Wno-pragmas"
open_solution "solution"
set_part {xc7z045ffg900-2}
create_clock -period 156MHz -name default
config_compile -name_max_length 1000
config_rtl -encoding onehot -reset control -reset_level low
#source "./top/solution/directives.tcl"
csim_design -compiler clang
csynth_design
cosim_design -trace_level all
#export_design -flow impl -rtl verilog -format ip_catalog
