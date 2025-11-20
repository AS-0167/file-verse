# Define the C++ compiler
CXX = g++

# Define compiler flags
CXXFLAGS = -std=c++17 -g -Wall -Iinclude -pthread

# List ONLY the necessary C++ source files here.
SRCS = src/main.cpp src/filesystem.cpp src/data_structures/hash_table.cpp

# Define the name of your final program
TARGET = run_ofs

# The default command to run when you just type "make"
all: $(TARGET)

# The rule for building the program from all the source files.
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# A rule to clean up compiled files and the final program
clean:
	rm -f $(TARGET)