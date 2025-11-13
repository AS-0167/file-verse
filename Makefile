# Define the C++ compiler
CXX = g++

# Define compiler flags
# -std=c++17: Use the C++17 standard
# -g: Add debug information
# -Wall: Show all warnings
# -Iinclude: Tell the compiler to look for header files in the 'include' directory
# -pthread: Link the POSIX threads library, necessary for std::thread
CXXFLAGS = -std=c++17 -g -Wall -Iinclude -pthread

# List ALL your C++ source files here.
SRCS = src/main.cpp src/filesystem.cpp src/data_structures/hash_table.cpp src/server.cpp src/data_structures/queue.cpp

# Define the name of your final program
TARGET = run_ofs

# The default command to run when you just type "make"
all: $(TARGET)

# The rule for building the program from all the source files.
# This links all the compiled code together into a single executable.
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# A rule to clean up compiled files and the final program
clean:
	rm -f $(TARGET)