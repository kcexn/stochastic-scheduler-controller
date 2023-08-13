CC = g++
CC_FLAGS = -Wall
INCLUDE_PATH = -I/workspaces/whisk-controller-dev/boost/boost_1_82_0/
LIBRARY_PATH = -L/usr/local/lib/
LD_FLAGS = -l boost_system -l boost_date_time -l boost_thread -l boost_chrono -l boost_context
SRC_DIR = ./src

# DEBUG SETTINGS
DEBUG_CC_FLAGS = -g -D DEBUG -Og
DEBUG_TARGET = controller-dbg
DEBUG_OBJECTS = echo-dbg.o echo-components-worker-dbg.o echo-components-reader-dbg.o echo-components-writer-dbg.o sctp-server-dbg.o


# NORMAL SETTINGS
TARGET = controller
OBJECTS = echo.o echo-components-worker.o echo-components-reader.o echo-components-writer.o sctp-server.o
REL_CC_FLAGS = -O2 -finline-atomics -finline-functions -finline-small-functions -flto

.PHONY: clean debug

# DEFAULT is normal settings.
$(TARGET): $(SRC_DIR)/controller/main.cpp $(OBJECTS)
	$(CC) $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(OBJECTS) -o $(TARGET) $(LIBRARY_PATH) $(LD_FLAGS)

echo-components-writer.o: $(SRC_DIR)/echo-app/components/echo_writer.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo_writer.cpp -o echo-components-writer.o

echo-components-reader.o: $(SRC_DIR)/echo-app/components/echo_reader.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo_reader.cpp -o echo-components-reader.o

echo-components-worker.o: $(SRC_DIR)/echo-app/components/echo-worker.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo-worker.cpp -o echo-components-worker.o

echo.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp -o echo.o

sctp-server.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CC) -c $(REL_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp


debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS)
	$(CC) $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS) -o $(DEBUG_TARGET) $(LIBRARY_PATH) $(LD_FLAGS)	

echo-components-writer-dbg.o: $(SRC_DIR)/echo-app/components/echo_writer.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo_writer.cpp -o echo-components-writer-dbg.o

echo-components-reader-dbg.o: $(SRC_DIR)/echo-app/components/echo_reader.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo_reader.cpp -o echo-components-reader-dbg.o

echo-components-worker-dbg.o: $(SRC_DIR)/echo-app/components/echo-worker.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/components/echo-worker.cpp -o echo-components-worker-dbg.o

echo-dbg.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp -o echo-dbg.o

sctp-server-dbg.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp -o sctp-server-dbg.o


clean:
	rm -f controller controller-dbg *.o
