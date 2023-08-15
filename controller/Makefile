CXX = g++
CXX_FLAGS = -Wall
INCLUDE_PATH = -I/workspaces/whisk-controller-dev/boost/boost_1_82_0/
LIBRARY_PATH = -L/usr/local/lib/
LD_FLAGS = -l boost_system -l boost_date_time -l boost_chrono -l boost_context
SRC_DIR = ./src

# DEBUG SETTINGS
DEBUG_CXX_FLAGS = -g -D DEBUG -Og
DEBUG_TARGET = controller-dbg
DEBUG_OBJECTS = echo-dbg.o echo-worker-dbg.o echo-reader-dbg.o echo-writer-dbg.o sctp-server-dbg.o uuid-common-utils-dbg.o echo-scheduler-dbg.o echo-application-dbg.o unix-server-dbg.o


# NORMAL SETTINGS
TARGET = controller
OBJECTS = echo.o echo-worker.o echo-reader.o echo-writer.o sctp-server.o uuid-common-utils.o echo-scheduler.o echo-application.o unix-server.o
REL_CXX_FLAGS = -O2 -finline-atomics -finline-functions -finline-small-functions -flto=auto -fuse-linker-plugin

.PHONY: clean debug

# DEFAULT is normal settings.
$(TARGET): $(SRC_DIR)/controller/main.cpp $(OBJECTS)
	$(CXX) $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(OBJECTS) -o $(TARGET) $(LIBRARY_PATH) $(LD_FLAGS)


%.o: $(SRC_DIR)/echo-app/components/%.cpp $(SRC_DIR)/echo-app/components/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

unix-server.o: $(SRC_DIR)/unix-server/unix-server.cpp $(SRC_DIR)/unix-server/unix-server.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/unix-server/unix-server.cpp -o unix-server.o

%-common-utils.o: $(SRC_DIR)/utils/%.cpp $(SRC_DIR)/utils/%.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

echo.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp -o echo.o

sctp-server.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CXX) -c $(REL_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp


debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS)
	$(CXX) $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS) -o $(DEBUG_TARGET) $(LIBRARY_PATH) $(LD_FLAGS)	

%-dbg.o: $(SRC_DIR)/echo-app/components/%.cpp $(SRC_DIR)/echo-app/components/%.hpp $(SRC_DIR)/echo-app/utils/common.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

unix-server-dbg.o: $(SRC_DIR)/unix-server/unix-server.cpp $(SRC_DIR)/unix-server/unix-server.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/unix-server/unix-server.cpp -o unix-server-dbg.o

%-common-utils-dbg.o: $(SRC_DIR)/utils/%.cpp $(SRC_DIR)/utils/%.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $< -o $@

echo-dbg.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp -o echo-dbg.o

sctp-server-dbg.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CXX) -c $(DEBUG_CXX_FLAGS) $(CXX_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp -o sctp-server-dbg.o


clean:
	rm -f controller controller-dbg *.o
