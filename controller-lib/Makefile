CXX = g++
AR = ar
CXX_FLAGS = -Wpedantic
LD_FLAGS = -lboost_system
LIBRARY_PATH = -L/usr/local/lib/ -Llib/
SRC_DIR = ./src
TESTS_DIR = ./tests
LIB_DIR = ./lib
INCLUDE_DIR = ./include
OBJ_DIR = ./objects
BIN_DIR = ./bin
VPATH = $(SRC_DIR)/ $(sort $(dir $(wildcard $(SRC_DIR)/*/))) $(sort $(dir $(wildcard $(SRC_DIR)/*/*/))) $(sort $(dir $(wildcard $(TESTS_DIR)/*/))) $(sort $(dir $(wildcard $(TESTS_DIR)/*/*/)))

OBJECTS = uuid unix-server
TESTS = uuid-tests
TARGET = owcontroller_utils

# DEBUG SETTINGS
DEBUG_CXX_FLAGS = -g -D DEBUG -Og
DEBUG_LD_FLAGS = $(LIBRARY_PATH) $(LD_FLAGS)
DEBUG_TARGET = $(addsuffix -dbg, $(addprefix $(BIN_DIR)/, $(TARGET)))
DEBUG_OBJECTS = $(addsuffix -dbg.o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))
DEBUG_TESTS = $(addsuffix -dbg.o, $(addprefix $(OBJ_DIR)/, $(TESTS)))

# RELEASE SETTINGS
REL_CXX_FLAGS = -O3 -flto=auto -fuse-linker-plugin
REL_LD_FLAGS = $(LIBRARY_PATH) $(LD_FLAGS)
REL_TARGET = $(addprefix $(BIN_DIR)/, $(TARGET))
REL_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))
REL_TESTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(TESTS)))

# SHARED LIBRARY SETTINGS
SHARED_CXX_FLAGS = -O3 -flto=auto -fuse-linker-plugin -fpic
STATIC_AR_FLAGS = rcs
SHARED_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))
SHARED_TARGET = $(addsuffix .so, $(addprefix $(LIB_DIR)/lib, $(TARGET)))
STATIC_TARGET = $(addsuffix .a, $(addprefix $(LIB_DIR)/lib, $(TARGET)))

.PHONY: clean debug shared

$(REL_TARGET): main.cpp $(REL_OBJECTS) $(REL_TESTS)
	$(CXX) $(REL_CXX_FLAGS) $(CXX_FLAGS) $^ -o $@ $(REL_LD_FLAGS)

$(OBJ_DIR)/%.o: %.cpp %.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $< -o $@

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): main.cpp $(DEBUG_OBJECTS) $(DEBUG_TESTS)
	$(CXX) $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $^ -o $@ $(DEBUG_LD_FLAGS)

$(OBJ_DIR)/%-dbg.o: %.cpp %.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $< -o $@

shared: $(INCLUDE_DIR) $(LIB_DIR)

$(INCLUDE_DIR): $(SHARED_TARGET) $(STATIC_TARGET)
	mkdir -p $(INCLUDE_DIR)
	cp $(SRC_DIR)/**/*.hpp $(INCLUDE_DIR)/

$(SHARED_TARGET): $(SHARED_OBJECTS)
	$(CXX) -shared $^ -o $@

$(STATIC_TARGET): $(SHARED_OBJECTS)
	$(AR) $(STATIC_AR_FLAGS) $@ $^

$(OBJ_DIR)/%.o: %.cpp %.hpp
	$(CXX) -c $(SHARED_CXX_FLAGS) $(CXX_FLAGS) $< -o $@

clean:
	rm -f $(OBJ_DIR)/* $(BIN_DIR)/* $(LIB_DIR)/*
	rm -rf $(INCLUDE_DIR)
