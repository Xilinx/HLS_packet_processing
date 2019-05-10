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


APPSOURCES = main.cpp app.cpp
EXECUTABLE = app
PLATFORM = ../../zc706

SDSFLAGS = -sds-pf ${PLATFORM}
ACCFLAGS = \
	-sds-hw ReadPacketInternal eth_interface.cpp -sds-end \
	-sds-hw WritePacketInternal eth_interface.cpp -sds-end \
	-sds-hw ProcessPacketInternal app.cpp -sds-end

CC = sds++ ${SDSFLAGS} ${ACCFLAGS}
CFLAGS = -std=c++11 -Wall -Wno-unknown-attributes -Wno-unused-variable -Wno-unused-label -Wno-unused-function -Wno-unknown-pragmas -I../common -I../../boost
#CFLAGS += -O3
CFLAGS += -g -O0

DEPFLAGS = -MMD -MP -MF"$(@:%.o=%.d)"
LFLAGS = -lrt
ifdef SW
CFLAGS += -mno-bitstream
LFLAGS += -mno-bitstream
endif

HLS_CFLAGS = -I`vivado_hls -r`/include
PCAP_FLAGS = -I/group/xrlabs/tools/x86_64/include -L/group/xrlabs/tools/x86_64/lib -Wl,-rpath=/group/xrlabs/tools/x86_64/lib -lpcap

OBJECTS := $(APPSOURCES:.cpp=.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all

all: ${EXECUTABLE}

${EXECUTABLE}: ${OBJECTS}
	${CC} ${LFLAGS} $^ -o $@

# ${EXECUTABLE}.sw: CC=arm-xilinx-linux-gnueabi-g++
# ${EXECUTABLE}.sw: ${OBJECTS}
# 	${CC}	${LFLAGS} $^ -o $@ 


${EXECUTABLE}.x86_sim: $(APPSOURCES)
	clang++ -DSIM ${HLS_CFLAGS} $(APPSOURCES) -g $(CFLAGS) ${LFLAGS} -o $@

${EXECUTABLE}.arm_sim: $(APPSOURCES)
	arm-xilinx-linux-gnueabi-g++ -DSIM ${HLS_CFLAGS} $(APPSOURCES) -g $(CFLAGS) ${LFLAGS} ${PCAP_FLAGS} -o $@

-include ${DEPS}

%.o: %.cpp
	${CC} ${CFLAGS} ${DEPFLAGS} -c $< -o $@

clean:
	${RM} ${EXECUTABLE} ${OBJECTS} ${DEPS}

ultraclean: clean
	${RM} ${EXECUTABLE}.bit 
	${RM} *.o
	${RM} -rf _sds sd_card
