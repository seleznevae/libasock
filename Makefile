
####### Compiler, tools and options

#EXTRA_CHECKING_POLICY = -fsanitize=address
#EXTRA_CHECKING_POLICY = -fsanitize=thread
ERROR_CHECKING_POLICY = -Werror $(EXTRA_CHECKING_POLICY)


CC            = gcc
CXX           = g++
DEFINES       =
OPTIMIZATIONS = #-O0
CFLAGS        = -pipe -Wall -Wextra -D_REENTRANT -fPIC -MMD $(OPTIMIZATIONS) $(ERROR_CHECKING_POLICY) $(DEFINES) 
CXXFLAGS      = -pipe  -Wall -Wextra -W -D_REENTRANT -fPIC -MMD $(OPTIMIZATIONS) $(ERROR_CHECKING_POLICY) -std=c++14 $(DEFINES)
INCPATH       = -I./src -I. -I./include
INSTALL_DIR   = cp -f -R
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
TAR           = tar -cf
COMPRESS      = gzip -9f
LFLAGS        = $(EXTRA_CHECKING_POLICY)
LIBS          = -lpthread #-lasan 


####### Dirs and modules

TARGET             = libasock.so

SRC_DIR   	       = ./src
BUILD_DIR          = ./build
BIN_DIR            = ./bin
LIB_DIR            = ./lib
UTILITY_DIRS       = $(BUILD_DIR) $(BIN_DIR)

TEST_SRC_DIR       = ./test
TEST_TARGET        = test_asock

####### Output


CPP_FILES         = $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)/*/*.cpp)
C_FILES           = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c) ./example/main.c
TEST_CPP_FILES    = $(wildcard $(TEST_SRC_DIR)/*.cpp) $(wildcard $(TEST_SRC_DIR)/*/*.cpp)

OBJCPP_FILES      = $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_FILES:.cpp=.cpp.o)))
TEST_OBJCPP_FILES = $(addprefix $(BUILD_DIR)/,$(notdir $(TEST_CPP_FILES:.cpp=.cpp.o)))
OBJC_FILES        = $(addprefix $(BUILD_DIR)/,$(notdir $(C_FILES:.c=.c.o)))

VPATH =  $(sort $(dir $(CPP_FILES) $(C_FILES) $(TEST_CPP_FILES))) # get dir-names of all src files (sort to remove duplicates)

####### Actions

.PHONY: clean checkdirs test

all: CXXFLAGS +=  -g #(temporary) for development purposes
all:  $(LIB_DIR)/$(TARGET)

debug: CXXFLAGS +=  -g -DDEBUG
debug: CFLAGS +=  -g -DDEBUG
debug: $(TARGET)
test: $(BIN_DIR)/$(TEST_TARGET)
test: CXXFLAGS +=  -g -DDEBUG
test: CFLAGS +=  -g -DDEBUG
#test: INCPATH += -I../../src
#test: INCPATH += -I./src

$(LIB_DIR)/$(TARGET): $(OBJCPP_FILES) $(OBJC_FILES)
	mkdir -p $(LIB_DIR)
	$(CXX) -shared $(LFLAGS) -o $@ $^ $(LIBS)

$(BIN_DIR)/$(TEST_TARGET): $(OBJCPP_FILES) $(TEST_OBJCPP_FILES) $(LIB_DIR)/$(TARGET)
	$(CXX)   $(LFLAGS)  -o $@ $^ -L./lib -lasock $(LIBS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCPATH) -c -o $@ $<

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(CFLAGS) $(INCPATH) -c -o $@ $<


clean:
	@rm -rf $(BUILD_DIR)/*.o
	@rm -rf $(BUILD_DIR)/*.d
	@rm -rf $(BIN_DIR)/*
	@rm -rf $(LIB_DIR)/*

-include $(OBJCPP_FILES:.o=.d)
-include $(OBJC_FILES:.o=.d)
-include $(TEST_OBJCPP_FILES:.o=.d)
