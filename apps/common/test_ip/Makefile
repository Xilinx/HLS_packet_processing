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

TESTS = test_serialize1 test_serialize2 test_serialize3 test_serialize4 test_serialize5 test_serialize6
TESTS += test_serialize_array1 test_serialize_array2 test_serialize_array3 test_serialize_array4 test_serialize_array5 test_serialize_array6 test_serialize_array7
TESTS += test_prepend_set1 test_prepend_set2 test_prepend_set3 test_prepend_set4 test_prepend_set5 test_prepend_set6
TESTS += test_truncate1  test_truncate2 test_truncate3 test_truncate4 test_truncate5 test_truncate6 test_truncate7
TESTS += test_stream1  test_stream2 test_stream3 test_stream4 test_stream5 test_stream6 test_stream7
#TESTS += test_parse1  test_parse2 test_parse3 test_parse4 test_parse5

all: $(TESTS)
	echo "TESTS COMPLETE"

CPP = g++
CFLAGS:=-std=c++11 -g -DHLS_NO_XIL_FPO_LIB -I`vivado_hls -r`/include -I .. -I../boost_1_60_0/ -DDEBUG -Wno-multichar

test_serialize_array%:: test_serialize_array.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_serialize_array.cpp
	@$@ | tail -1

test_serialize%:: test_serialize.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_serialize.cpp
	@$@ | tail -1

test_prepend_set%:: test_prepend.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_prepend.cpp
	@$@ | tail -1

test_truncate%:: test_truncate.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_truncate.cpp
	@$@ | tail -1

test_parse%:: test_parse.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_parse.cpp
	@$@ | tail -1

test_stream%:: test_stream.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_stream.cpp
	@$@ | tail -1

test_reader%:: test_reader.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_reader.cpp
	@$@ | tail -1

test_writer%:: test_writer.cpp ../ip.hpp
	@$(CPP) $(CFLAGS) -DTEST=$@ -o $@ test_writer.cpp
	@$@ | tail -1

clean:
	rm $(TESTS)
