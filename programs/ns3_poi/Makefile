CXX := g++
CXXFLAGS := -std=c++14

RM=rm

SRC = src/test.cpp
OBJ = obj/test.o
BIN = bin/test

INC := -I/Users/Sensei/temp/ns-allinone-3.29/ns-3.29/build/
LIB := -L/Users/Sensei/temp/ns-allinone-3.29/ns-3.29/build/lib/

all: $(OBJ)
	$(CXX) $(CXXFLAGS) $(INC) $^ -o $@ $(LIB)

-include $(DEP_SERVER)
-include $(DEP_CLIENT)

obj/%.o: src/%.cpp Makefile
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@ 

clean:
	$(RM) obj/* bin/*
