# OFS Server Makefile for BSCS24115 Phase 1

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./source/include
LDFLAGS = 

# Directories
SRC_DIR = source
BUILD_DIR = build
BIN_DIR = bin
CORE_DIR = $(SRC_DIR)/core
SERVER_DIR = $(SRC_DIR)/server

# Source files (adjust based on your actual file structure)
# If you have ofs_core_part1.cpp, part2.cpp, etc., list them here
# If you have a single ofs_core.cpp, use that instead
CORE_SRC = $(wildcard $(CORE_DIR)/ofs_core*.cpp)
SERVER_MAIN = $(SRC_DIR)/server_main.cpp

# Object files
CORE_OBJ = $(CORE_SRC:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
SERVER_OBJ = $(BUILD_DIR)/server_main.o

# Target executable
TARGET = $(BIN_DIR)/ofs_server

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BIN_DIR)
	@echo "Build directories created"

# Link
$(TARGET): $(CORE_OBJ) $(SERVER_OBJ)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"

# Compile core files
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile server main
$(BUILD_DIR)/server_main.o: $(SERVER_MAIN)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Clean complete"

# Clean everything including .omni files
distclean: clean
	rm -f *.omni
	@echo "✓ Deep clean complete"

# Format a new file system
format:
	@echo "Formatting new file system..."
	./$(TARGET) BSCS24115.omni compiled/default.uconf --format

# Run server (load existing file system)
run: $(TARGET)
	@echo "Starting server..."
	./$(TARGET) BSCS24115.omni compiled/default.uconf

# Run with formatting first
run-format: $(TARGET)
	@echo "Starting server (with format)..."
	./$(TARGET) BSCS24115.omni compiled/default.uconf --format

# Help target
help:
	@echo "OFS Server Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build the server (default)"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove build artifacts and .omni files"
	@echo "  format       - Format a new file system"
	@echo "  run          - Build and run server (load existing)"
	@echo "  run-format   - Build and run server (format first)"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build"
	@echo "  make run-format   # First time (creates file system)"
	@echo "  make run          # Subsequent runs (loads existing)"

.PHONY: all clean distclean format run run-format help directories