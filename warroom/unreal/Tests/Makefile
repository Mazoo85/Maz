# Builds and runs the simulation tests without Unreal:  make && ./sim_tests ../../data
CXX ?= g++
CXXFLAGS ?= -std=c++20 -O1 -Wall -Wextra -Wno-unused-parameter -I../Source/WarRoomSim/Public
SRC := $(filter-out %WarRoomSimModule.cpp,$(wildcard ../Source/WarRoomSim/Private/*.cpp))
sim_tests: sim_tests.cpp $(SRC) $(wildcard ../Source/WarRoomSim/Public/WarRoomSim/*.h)
	$(CXX) $(CXXFLAGS) -o $@ sim_tests.cpp $(SRC)
.PHONY: run clean
run: sim_tests
	./sim_tests ../../data
clean:
	rm -f sim_tests
