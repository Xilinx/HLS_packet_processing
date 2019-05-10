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

set_directive_interface -mode s_axilite "read_and_process_packet" packets_sent
set_directive_interface -mode s_axilite "read_and_process_packet" packets_received
set_directive_interface -mode s_axilite "read_and_process_packet" count
set_directive_interface -mode s_axilite "read_and_process_packet" ipAddress
set_directive_interface -mode s_axilite "read_and_process_packet" validMessage
set_directive_interface -mode s_axilite "read_and_process_packet" size
set_directive_interface -mode s_axilite "read_and_process_packet" b
set_directive_interface -mode s_axilite "read_and_process_packet" events_completed
set_directive_interface -mode s_axilite "read_and_process_packet" destIP
set_directive_interface -mode s_axilite "read_and_process_packet" destPort
set_directive_interface -mode s_axilite "read_and_process_packet" message
set_directive_interface -mode s_axilite "read_and_process_packet" publishes_sent
set_directive_interface -mode s_axilite "read_and_process_packet" reset
set_directive_interface -mode m_axi -offset slave "read_and_process_packet" networkIOP -bundle=networkIOP
set_directive_interface -mode s_axilite "read_and_process_packet" topicID
set_directive_interface -mode s_axilite "read_and_process_packet" _verbose
set_directive_interface -mode s_axilite "read_and_process_packet" macAddress
set_directive_interface -mode s_axilite "read_and_process_packet" i
set_directive_interface -mode s_axilite "read_and_process_packet" messageValid
set_directive_interface -mode s_axilite "read_and_process_packet" qos
set_directive_interface -mode s_axilite "read_and_process_packet" return
set_directive_latency -min 1 read_and_process_packet
