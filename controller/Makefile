CC = g++
CC_FLAGS = -Wall
INCLUDE_PATH = -I/workspaces/whisk-controller-dev/boost/boost_1_82_0/ \
-I/workspaces/whisk-controller-dev/controller/src/echo-app/ \
-I/workspaces/whisk-controller-dev/controller/src/sctp-server/ \
-I/workspaces/whisk-controller-dev/controller/src/utils/
LIBRARY_PATH = -L/usr/local/lib/
LD_FLAGS = -l boost_system -l boost_date_time -l boost_thread -l boost_chrono -l boost_context
SRC_DIR = ./src

# DEBUG SETTINGS
DEBUG_CC_FLAGS = -g -D DEBUG
DEBUG_TARGET = controller-dbg
DEBUG_OBJECTS = echo-dbg.o sctp-server-dbg.o


# NORMAL SETTINGS
TARGET = controller
OBJECTS = echo.o sctp-server.o

.PHONY: clean debug

# DEFAULT is normal settings.
$(TARGET): $(SRC_DIR)/controller/main.cpp $(OBJECTS)
	$(CC) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(OBJECTS) -o $(TARGET) $(LIBRARY_PATH) $(LD_FLAGS)

echo.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp sctp-server.o
	$(CC) -c $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp

sctp-server.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CC) -c $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp


debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS)
	$(CC) $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(DEBUG_OBJECTS) -o $(DEBUG_TARGET) $(LIBRARY_PATH) $(LD_FLAGS)	

echo-dbg.o: $(SRC_DIR)/echo-app/echo.cpp $(SRC_DIR)/echo-app/echo.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/echo-app/echo.cpp -o echo-dbg.o

sctp-server-dbg.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp $(SRC_DIR)/sctp-server/sctp.hpp
	$(CC) -c $(DEBUG_CC_FLAGS) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp -o sctp-server-dbg.o


clean:
	rm -f controller controller-dbg *.o
