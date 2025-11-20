# Use the C++ compiler as the main compiler/linker
CXX = g++
# Use the C compiler for C files
CC = gcc

# Define compiler flags for C++ and C
CXXFLAGS = -std=c++17 -g -Wall -I./source/include -pthread
CFLAGS = -Wall -g -I./source/include

# List ALL the core C source files
C_SOURCES = source/data_structures/hash_table.c source/data_structures/fs_tree.c source/data_structures/bitmap.c source/data_structures/queue.c \
            source/core/fs_core.c source/core/user_management.c source/core/file_operations.c source/core/directory_operations.c source/core/security.c

# List our new C++ source file that contains the main() function for the server
CXX_SOURCES = source/server/web_server.cpp

# Convert source file names to object file names (.o)
C_OBJECTS = $(C_SOURCES:.c=.o)
CXX_OBJECTS = $(CXX_SOURCES:.cpp=.o)

# Executable name
TARGET = ofs_server

# Default target: build the executable
all: $(TARGET)

# Link the executable using the C++ compiler
$(TARGET): $(C_OBJECTS) $(CXX_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(C_OBJECTS) $(CXX_OBJECTS)

# Rule to compile C++ source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile C source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(C_OBJECTS) $(CXX_OBJECTS) $(TARGET) ofs_format_tool

# Rule to format a new file system.
# This compiles the original main.c and the core C files to create a temporary tool.
format:
	$(CC) $(CFLAGS) -o ofs_format_tool source/server/main.c $(C_SOURCES)
	./ofs_format_tool format compiled/sample.omni default.uconf
	rm -f ofs_format_tool

# Rule to run the server
run: all
	./$(TARGET)