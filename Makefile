CXX = g++
# -pthread is required for httplib on Linux
CXXFLAGS = -std=c++17 -Wall -pthread -I./include 

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/Main.cpp \
       $(SRC_DIR)/FileSystem.cpp \
       $(SRC_DIR)/data_structures/UserMap.cpp \
       $(SRC_DIR)/data_structures/RequestQueue.cpp

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Executable Name
TARGET = $(BIN_DIR)/ofs_server

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	# Removed -lws2_32 because you are on Linux
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) my_ofs.omni

run: all
	./$(TARGET)

.PHONY: all clean run