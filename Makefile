
# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -I./source/include
LDFLAGS = -lpthread -ljson-c

# Source directories
SRCDIR_DS = source/data_structures
SRCDIR_CORE = source/core
SRCDIR_SERVER = source/server

# Source files
# NOTE: We add ALL .c files here
# (This is the only line you need to change in the Makefile)
SOURCES = $(SRCDIR_DS)/hash_table.c $(SRCDIR_DS)/fs_tree.c $(SRCDIR_DS)/bitmap.c $(SRCDIR_DS)/queue.c \
          $(SRCDIR_CORE)/fs_core.c $(SRCDIR_CORE)/user_management.c $(SRCDIR_CORE)/file_operations.c $(SRCDIR_CORE)/directory_operations.c $(SRCDIR_CORE)/security.c \
          $(SRCDIR_SERVER)/socket_server.c $(SRCDIR_SERVER)/main.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Executable name
TARGET = ofs_server

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rule to format a new file system
format:
	@if [ ! -f $(TARGET) ]; then make; fi
	./$(TARGET) format compiled/sample.omni default.uconf

# Rule to run the server
run:
	@if [ ! -f $(TARGET) ]; then make; fi
	./$(TARGET) compiled/sample.omni default.uconf