CXXFLAGS = -std=c++11 -Wall -Werror

# Set these
CATCH_DIR = ../catch
HLS_INCLUDE_DIR = /u/tsqahfus/local/tools/xilinx/Vivado/2018.3/include/

SRC_DIR = src
TEST_DIR = test
TL_DIR = tl
BUILD_DIR = build
BIN_DIR = bin

INCLUDE_DIRS = $(SRC_DIR) $(TEST_DIR) $(TL_DIR) $(CATCH_DIR) $(HLS_INCLUDE_DIR)
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

BINS = $(addprefix $(BIN_DIR)/,test_main tl_main)

ARTIFACTS = $(OBJS) $(TEST_OBJS) $(TL_OBJS) $(BINS)

all: $(OBJS) $(TEST_OBJS) $(BINS)

$(BIN_DIR)/test_main: $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^

$(BIN_DIR)/tl_main: $(TL_OBJS)
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

.PHONY: clean
clean:
	rm -f $(ARTIFACTS)
