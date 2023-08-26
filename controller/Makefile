CXX = g++
CXX_FLAGS = -Wall
INCLUDE_PATH = -I/workspaces/whisk-controller-dev/boost/boost_1_82_0/
LIBRARY_PATH = -L/usr/local/lib/
LD_FLAGS = -l boost_system -l boost_date_time -l boost_chrono -l boost_json -l boost_context
SRC_DIR = ./src
OBJ_DIR = ./objects

# DEBUG SETTINGS
DEBUG_CXX_FLAGS = -g -D DEBUG -Og
DEBUG_TARGET = controller-dbg
DEBUG_OBJECTS = echo-dbg.o echo-worker-dbg.o echo-reader-dbg.o \
echo-writer-dbg.o sctp-server-dbg.o uuid-common-utils-dbg.o \
echo-scheduler-dbg.o echo-application-dbg.o unix-server-dbg.o \
http-server-dbg.o controller-app-dbg.o run-dbg.o init-dbg.o

# NORMAL SETTINGS
REL_CXX_FLAGS = -O3 -finline-atomics -flto=auto -fuse-linker-plugin
TARGET = controller
OBJECTS = echo.o echo-worker.o echo-reader.o echo-writer.o \
sctp-server.o uuid-common-utils.o echo-scheduler.o \
echo-application.o unix-server.o http-server.o controller-app.o run.o \
init.o

.PHONY: clean debug

# DEFAULT is normal settings.
$(TARGET): $(SRC_DIR)/controller/main.cpp $(foreach OBJ,$(OBJECTS),$(OBJ_DIR)/$(OBJ) )
	$(CXX) $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $^ -o $(TARGET) $(LIBRARY_PATH) $(LD_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/echo-app/components/%.cpp $(SRC_DIR)/echo-app/components/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/http-server/%.cpp $(SRC_DIR)/http-server/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/unix-server.o: $(SRC_DIR)/unix-server/unix-server.cpp $(SRC_DIR)/unix-server/unix-server.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/controller/*/%.cpp $(SRC_DIR)/controller/*/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/controller/*/*/%.cpp $(SRC_DIR)/controller/*/*/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%-common-utils.o: $(SRC_DIR)/utils/%.cpp $(SRC_DIR)/utils/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/echo.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/sctp-server.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@


debug: $(DEBUG_TARGET)


$(DEBUG_TARGET): $(SRC_DIR)/controller/main.cpp $(foreach OBJ,$(DEBUG_OBJECTS),$(OBJ_DIR)/$(OBJ) )
	$(CXX) $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $^ -o $(DEBUG_TARGET) $(LIBRARY_PATH) $(LD_FLAGS)	

$(OBJ_DIR)/%-dbg.o: $(SRC_DIR)/echo-app/components/%.cpp $(SRC_DIR)/echo-app/components/%.hpp $(SRC_DIR)/echo-app/utils/common.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%-dbg.o: $(SRC_DIR)/http-server/%.cpp $(SRC_DIR)/http-server/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/unix-server-dbg.o: $(SRC_DIR)/unix-server/unix-server.cpp $(SRC_DIR)/unix-server/unix-server.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%-dbg.o: $(SRC_DIR)/controller/*/%.cpp $(SRC_DIR)/controller/*/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%-dbg.o: $(SRC_DIR)/controller/*/*/%.cpp $(SRC_DIR)/controller/*/*/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/%-common-utils-dbg.o: $(SRC_DIR)/utils/%.cpp $(SRC_DIR)/utils/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/echo-dbg.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

$(OBJ_DIR)/sctp-server-dbg.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@


clean:
	rm -f controller controller-dbg $(OBJ_DIR)/*.o
