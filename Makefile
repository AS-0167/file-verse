CXX=g++
CXXFLAGS=-Wall -O2 -std=c++17 -Iinclude
SRC=$(wildcard source/core/*.cpp source/data_structures/*.cpp source/server/*.cpp)
OBJ=$(SRC:.cpp=.o)
TARGET=ofs_server

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(TARGET)
