CC = gcc
CFLAGS = -Wpedantic
LD_FLAGS = -lcurl
SRC_DIR = ./src
INCLUDE_DIR = ./include
LIB_DIR = ./lib
OBJ_DIR = ./objects
VPATH = $(SRC_DIR)

OBJECTS = lua-easy-curl
TARGET = easyCurl

SHARED_CFLAGS = -O3 -flto=auto -fuse-linker-plugin -fpic
SHARED_OBJECTS = $(addsuffix .o, $(addprefix $(OBJ_DIR)/, $(OBJECTS)))
SHARED_TARGET = $(addsuffix .so, $(addprefix $(LIB_DIR)/, $(TARGET)))

.PHONY: shared

shared: $(SHARED_TARGET)
  
$(SHARED_TARGET): $(SHARED_OBJECTS)
	$(CC) -shared $^ -o $@ $(LD_FLAGS)

$(OBJ_DIR)/%.o: %.c %.h
	$(CC) -c $(SHARED_CFLAGS) $(CFLAGS) $< -o $@
