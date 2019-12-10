#
#    Copyright 2017 Two Sigma Open Source, LLC
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.


open_project mold_example
set_top mold_example

set cppflags "-I ../src -I ../../catch -std=gnu++11"
add_files mold_example.cc -cflags "$cppflags"
add_files -tb test_main.cc -cflags "$cppflags"
add_files -tb mold_example_test.cc -cflags "$cppflags"

open_solution "solution"
set_part {xcvu9p-flgb2104-3-e}
create_clock -period 3.103 -name default
config_rtl -reset_level low
csim_design -compiler clang
csynth_design
cosim_design -compiler clang -trace_level all -ldflags "-L/usr/lib/x86_64-linux-gnu/ -B/usr/lib/x86_64-linux-gnu/"
quit
