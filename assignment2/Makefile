CPPC=g++
CPPFLAGS=-std=c++14 -Wall
RM=rm

SRC_SERVER := src/server.cpp
OBJ_SERVER := $(patsubst src/%.cpp, obj/%.o, $(SRC_SERVER))
DEP_SERVER := $(patsubst src/%.cpp, obj/%.d, $(SRC_SERVER))

SRC_CLIENT := src/client.cpp
OBJ_CLIENT := $(patsubst src/%.cpp, obj/%.o, $(SRC_CLIENT))
DEP_CLIENT := $(patsubst src/%.cpp, obj/%.d, $(SRC_CLIENT))

SERVER := bin/server
CLIENT := bin/client

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
