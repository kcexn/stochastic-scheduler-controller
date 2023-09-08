CXX = g++
AR = ar
CXX_FLAGS = -Wpedantic
LIBRARY_PATH = -L/usr/local/lib/ -Llib/
SRC_DIR = ./src
LIB_DIR = ./lib
INCLUDE_DIR = ./include
OBJ_DIR = ./objects
BIN_DIR = ./bin

OBJECTS = uuid
TARGET = owcontroller_utils

# DEBUG SETTINGS
DEBUG_CXX_FLAGS = -g -D DEBUG -Og
DEBUG_LD_FLAGS = $(LIBRARY_PATH)
DEBUG_TARGET = $(addsuffix -dbg, $(addprefix $(BIN_DIR)/, $(TARGET)))
DEBUG_OBJECTS = $(addsuffix -dbg.o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))

# RELEASE SETTINGS
REL_CXX_FLAGS = -O3 -flto=auto -fuse-linker-plugin
REL_LD_FLAGS = $(LIBRARY_PATH)
REL_TARGET = $(addprefix $(BIN_DIR)/, $(TARGET))
REL_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))

# SHARED LIBRARY SETTINGS
SHARED_CXX_FLAGS = -O3 -flto=auto -fuse-linker-plugin -fpic
STATIC_AR_FLAGS = rcs
SHARED_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))
SHARED_TARGET = $(addsuffix .so, $(addprefix $(LIB_DIR)/lib, $(TARGET)))
STATIC_TARGET = $(addsuffix .a, $(addprefix $(LIB_DIR)/lib, $(TARGET)))

.PHONY: clean debug shared

$(REL_TARGET): $(SRC_DIR)/main.cpp $(REL_OBJECTS)
	$(CXX) $(REL_CXX_FLAGS) $(CXX_FLAGS) $^ -o $@ $(REL_LD_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/*/%.cpp $(SRC_DIR)/*/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $< -o $@


debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRC_DIR)/main.cpp $(DEBUG_OBJECTS)
	$(CXX) $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $^ -o $@ $(DEBUG_LD_FLAGS)

$(OBJ_DIR)/%-dbg.o: $(SRC_DIR)/**/%.cpp $(SRC_DIR)/**/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $< -o $@

shared: $(SHARED_TARGET) $(STATIC_TARGET)
	cp $(SRC_DIR)/**/*.hpp $(INCLUDE_DIR)/

$(SHARED_TARGET): $(SHARED_OBJECTS)
	$(CXX) -shared $^ -o $@

$(STATIC_TARGET): $(SHARED_OBJECTS)
	$(AR) $(STATIC_AR_FLAGS) $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/**/%.cpp $(SRC_DIR)/**/%.hpp
	$(CXX) -c $(SHARED_CXX_FLAGS) $(CXX_FLAGS) $< -o $@

clean:
	rm -f $(OBJ_DIR)/* $(BIN_DIR)/* $(LIB_DIR)/* $(INCLUDE_DIR)/*
