CPPC=g++
CPPFLAGS=-std=c++14 -Wall
RM=rm

SRC := src/server.cpp src/main.cpp
OBJ := $(patsubst src/%.cpp, obj/%.o, $(SRC))
DEP := $(patsubst src/%.cpp, obj/%.d, $(SRC))
BIN := bin/server

INC := #-I./include/

all: $(BIN)

$(BIN): $(OBJ)
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) $^ -o $@ $(LFLAGS)

-include $(DEP)

obj/%.o: src/%.cpp Makefile
	$(CPPC) $(CPPFLAGS) $(INC) $(LIB) -c $< -o $@ $(LFLAGS)

clean:
	$(RM) $(OBJ) $(BIN) $(DEP)

bin/%.o: src/%.cpp
