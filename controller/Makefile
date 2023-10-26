CXX = g++
CXX_FLAGS = -Wpedantic
INCLUDE_PATH = -I../boost/boost_1_82_0/ -I../controller-lib/include/
LIBRARY_PATH = -L/usr/local/lib/ -L../controller-lib/lib/
LD_FLAGS = -l boost_system -l boost_json -l boost_context -l:libowcontroller_utils.a -lpthread
SRC_DIR = ./src
BIN_DIR = ./bin
OBJ_DIR = ./objects
VPATH = $(sort $(dir $(wildcard $(SRC_DIR)/*/))) $(sort $(dir $(wildcard $(SRC_DIR)/*/*/))) $(sort $(dir $(wildcard $(SRC_DIR)/*/*/*/)))

TARGET = controller
OBJECTS = controller-app run init \
controller-io execution-context action-manifest action-relation thread-controls

# DEBUG SETTINGS
DEBUG_CXX_FLAGS = -g -D DEBUG -Og
DEBUG_TARGET = $(addsuffix -dbg, $(addprefix $(BIN_DIR)/, $(TARGET)))
DEBUG_OBJECTS = $(addsuffix -dbg.o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))

# NORMAL SETTINGS
REL_CXX_FLAGS = -O3 -D NDEBUG -flto=auto -fuse-linker-plugin \
				-fmodulo-sched -fmodulo-sched-allow-regmoves \
				-fsched-pressure -fipa-pta -freschedule-modulo-scheduled-loops \
				-march=x86-64-v2 -mtune=generic
# REL_CXX_FLAGS = -g -Og
REL_TARGET = $(addprefix $(BIN_DIR)/, $(TARGET))
REL_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))

.PHONY: clean debug

# DEFAULT is normal settings.
$(REL_TARGET): main.cpp $(REL_OBJECTS)
	$(CXX) $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $^ -o $@ $(LIBRARY_PATH) $(LD_FLAGS)

$(OBJ_DIR)/%.o: %.cpp %.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@


debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): main.cpp $(DEBUG_OBJECTS)
	$(CXX) $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $^ -o $(DEBUG_TARGET) $(LIBRARY_PATH) $(LD_FLAGS)

$(OBJ_DIR)/%-dbg.o: %.cpp %.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/*
