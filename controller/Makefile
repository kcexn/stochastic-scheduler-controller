CC = g++
CC_FLAGS = -Wall
INCLUDE_PATH = -I/workspaces/whisk-controller-dev/boost/boost_1_82_0/ -I/workspaces/whisk-controller-dev/controller/src/sctp-server/
LIBRARY_PATH = -L/usr/local/lib/
LD_FLAGS = -l boost_system -l boost_date_time -l boost_thread -l boost_chrono
SRC_DIR = ./src

OBJECTS = sctp-server.o

main: $(SRC_DIR)/controller/main.cpp $(OBJECTS)
	$(CC) $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/controller/main.cpp $(OBJECTS) -o main $(LIBRARY_PATH) $(LD_FLAGS)

sctp-server.o: $(SRC_DIR)/sctp-server/sctp-server.cpp  $(SRC_DIR)/sctp-server/sctp-server.hpp
	$(CC) -c $(CC_FLAGS) $(INCLUDE_PATH) $(SRC_DIR)/sctp-server/sctp-server.cpp
	
.PHONY: clean
clean:
	rm -f main *.o