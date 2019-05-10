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

open_project test
set_top test
add_files test.cpp -cflags "-I../common"
#add_files -tb testtest.cpp -cflags "-I../common"
open_solution "solution" -reset
set_part { xc7z020clg400-1 }
# synthesis directives
create_clock -period 10.000000
config_rtl -reset_level low
#set_directive_data_pack process_packet inBuf

# end synthesis directives
#csim_design
csynth_design
#cosim_design
