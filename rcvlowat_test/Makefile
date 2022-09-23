CC=gcc
NORMAL_FLAGS := -std=c11
PEDANTIC_FLAGS := -g

RM=rm

ifeq ($(DEBUG),1)
	CFLAGS := $(NORMAL_FLAGS) $(PEDANTIC_FLAGS)
endif

ifneq ($(OS),Windows_NT)
	uname_os := $(shell uname)
	ifneq ($(uname_os),Darwin)
		CPPFLAGS += -I/usr/include/kqueue
		LFLAGS += -lkqueue -lpthread
	endif
endif

SRC_SERVER := src/server.c
SRC_CLIENT := src/client.c
SERVER := bin/server
CLIENT := bin/client

OBJ_SERVER := $(patsubst src/%.c, obj/%.o, $(SRC_SERVER))
DEP_SERVER := $(patsubst src/%.c, obj/%.d, $(SRC_SERVER))

OBJ_CLIENT := $(patsubst src/%.c, obj/%.o, $(SRC_CLIENT))
DEP_CLIENT := $(patsubst src/%.c, obj/%.d, $(SRC_CLIENT))

INC := #-I./include/

all: $(SERVER) $(CLIENT)

server: $(SERVER)

client: $(CLIENT)

$(SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

$(CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

-include $(DEP_SERVER)
-include $(DEP_CLIENT)

obj/%.o: src/%.c Makefile
	$(CC) $(CFLAGS) -MMD -MP $(INC) $(LIB) -c $< -o $@ $(LFLAGS)

clean:
	$(RM) obj/* bin/*

bin/%.o: src/%.c
