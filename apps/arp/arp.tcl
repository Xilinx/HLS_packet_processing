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

set boost "../../boost"
#set boost "/proj/xsjhdstaff2/stephenn/p4_HEAD/Rodin/HEAD/src/ext/Boost/boost_1_64_0"
open_project arp_proj
set_top arp_ingress
add_files app.cpp -cflags "-std=c++11 -Wall -Wno-unknown-attributes -Wno-unused-variable -Wno-pragmas -D __SDSCC__ -I ../common -I$boost -w"
add_files -tb main.cpp -cflags "-std=c++11 -Wall -Wno-unknown-attributes -Wno-unused-variable -Wno-pragmas -D __SDSCC__ -I ../common -I$boost -w"
open_solution "solution" -reset
set_part { xc7z045ffg900-2 }
# synthesis directives
create_clock -period 8.000000
config_rtl -reset_level low
# end synthesis directives
csim_design -compiler clang
csynth_design
cosim_design -compiler clang
#export_design -acc
