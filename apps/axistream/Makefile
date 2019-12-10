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


CXXFLAGS = -std=c++11 -Wall -Werror -Wno-unused-label

# Set these
CATCH_DIR = ../catch
HLS_INCLUDE_DIR = /home/tslocal/xilinx/Vivado/2018.3/include/

SRC_DIR = src
TEST_DIR = test
TL_DIR = tl
MOLD_EXAMPLE_DIR = mold_example
BUILD_DIR = build
BIN_DIR = bin

INCLUDE_DIRS = $(SRC_DIR) $(TEST_DIR) $(TL_DIR) $(MOLD_EXAMPLE_DIR) $(CATCH_DIR) $(HLS_INCLUDE_DIR)
INCLUDE_FLAGS = $(addprefix -I,$(INCLUDE_DIRS))

SRCS = $(wildcard src/*.cc)
HDRS = $(wildcard src/*.hh)
OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(patsubst %.cc,%.o,$(SRCS))))

TEST_SRCS = $(wildcard test/*.cc)
TEST_HDRS = $(wildcard test/*.hh)
TEST_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(patsubst %.cc,%.o,$(TEST_SRCS))))

TL_SRCS = $(wildcard tl/*.cc)
TL_HDRS = $(wildcard tl/*.hh)
TL_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(patsubst %.cc,%.o,$(TL_SRCS))))

MOLD_EXAMPLE_SRCS = $(wildcard mold_example/*.cc)
MOLD_EXAMPLE_HDRS = $(wildcard mold_example/*.hh)
MOLD_EXAMPLE_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(patsubst %.cc,%.o,$(MOLD_EXAMPLE_SRCS))))

BINS = $(addprefix $(BIN_DIR)/,test_main tl_main mold_example_main)

ARTIFACTS = $(OBJS) $(TEST_OBJS) $(TL_OBJS) $(MOLD_EXAMPLE_OBJS) $(BINS)

all: $(OBJS) $(TEST_OBJS) $(BINS)

$(BIN_DIR)/test_main: $(OBJS) $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^

$(BIN_DIR)/tl_main: $(OBJS) $(TL_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^

$(BIN_DIR)/mold_example_main: $(OBJS) $(MOLD_EXAMPLE_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc $(HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDE_FLAGS)

$(BUILD_DIR)/%.o : $(TEST_DIR)/%.cc $(HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDE_FLAGS)

$(BUILD_DIR)/%.o : $(TL_DIR)/%.cc $(HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDE_FLAGS)

$(BUILD_DIR)/%.o : $(MOLD_EXAMPLE_DIR)/%.cc $(HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDE_FLAGS)

.PHONY: clean
clean:
	rm -f $(ARTIFACTS)
