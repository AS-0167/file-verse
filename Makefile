<<<<<<< HEAD
CXX=g++
CXXFLAGS=-Wall -O2 -std=c++17 -Iinclude
SRC=$(wildcard source/core/*.cpp source/data_structures/*.cpp source/server/*.cpp)
OBJ=$(SRC:.cpp=.o)
TARGET=ofs_server

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(TARGET)
=======
# OFS Makefile
# Student ID: BSCS24115

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Isource/include
TARGET = ofs_test

# Source files
SOURCES = source/core/ofs_core.cpp \
          source/main.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) BSCS24115.omni

# Run the test program
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CXXFLAGS += -g -O0
debug: $(TARGET)

# Release build
release: CXXFLAGS += -O3
release: $(TARGET)

# Install dependencies (if needed)
deps:
	sudo apt-get update
	sudo apt-get install build-essential

# Show file structure
structure:
	@echo "Project Structure:"
	@echo "file-verse/"
	@echo "├── Makefile"
	@echo "├── source/"
	@echo "│   ├── main.cpp"
	@echo "│   ├── include/"
	@echo "│   │   ├── odf_types.hpp"
	@echo "│   │   └── ofs_core.hpp"
	@echo "│   └── core/"
	@echo "│       └── ofs_core.cpp"
	@echo "└── compiled/"
	@echo "    └── default.uconf"

.PHONY: all clean run debug release deps structure
>>>>>>> d45769e (Initial commit for phase1)
