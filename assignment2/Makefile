CPPC=g++
NORMAL_FLAGS := -std=c++14
PEDANTIC_FLAGS := -g

RM=rm

ifeq ($(DEBUG), )
	CPPFLAGS := $(NORMAL_FLAGS) $(PEDANTIC_FLAGS)
else
	CPPFLAGS := $(NORMAL_FLAGS)
endif

CPPFLAGS += -I/usr/include/kqueue
LFLAGS += -lkqueue

SRC_SERVER := src/server.cpp
SRC_CLIENT := src/client.cpp

ifeq ($(PROTO), TCP)
	SRC_SERVER += src/client_connection_tcp.cpp src/server_tcp.cpp
	SRC_CLIENT += src/client_tcp.cpp
	SERVER := bin/server_tcp
	CLIENT := bin/client_tcp
else
	SRC_SERVER += src/client_connection_udp.cpp src/server_udp.cpp
	SRC_CLIENT += src/client_udp.cpp
	SERVER := bin/server_udp
	CLIENT := bin/client_udp
endif

OBJ_SERVER := $(patsubst src/%.cpp, obj/%.o, $(SRC_SERVER))
DEP_SERVER := $(patsubst src/%.cpp, obj/%.d, $(SRC_SERVER))

OBJ_CLIENT := $(patsubst src/%.cpp, obj/%.o, $(SRC_CLIENT))
DEP_CLIENT := $(patsubst src/%.cpp, obj/%.d, $(SRC_CLIENT))

INC := #-I./include/

all: $(SERVER) $(CLIENT)

server: $(SERVER)

client: $(CLIENT)

$(SERVER): $(OBJ_SERVER)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

$(CLIENT): $(OBJ_CLIENT)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

-include $(DEP_SERVER)
-include $(DEP_CLIENT)

obj/%.o: src/%.cpp Makefile
	$(CPPC) $(CPPFLAGS) -MMD -MP $(INC) $(LIB) -c $< -o $@ $(LFLAGS)

clean:
	$(RM) $(OBJ_CLIENT) $(BIN_CLIENT) $(DEP_CLIENT) $(OBJ_SERVER) $(BIN_SERVER) $(DEP_SERVER)

bin/%.o: src/%.cpp
