# Use the C++ compiler
CXX = g++
# CXXFLAGS: -Wall (warnings), -g (debug symbols), -std=c++17, -pthread (for threads)
CXXFLAGS = -Wall -g -std=c++17 -pthread
# Tell the compiler to look for headers in the 'source' directory
CPPFLAGS = -I./source

# --- EXPLICITLY DEFINE ALL OBJECT FILES ---
SERVER_OBJECTS = compiled/source/main.o compiled/source/server/server.o compiled/source/core/filesystem.o compiled/source/core/queue.o compiled/source/core/hash_table.o
CLIENT_OBJECTS = compiled/source/client/client.o

# Default 'make' command builds both targets
all: compiled/server compiled/client

# --- LINKING RULES ---
# Rule to link the SERVER executable
compiled/server: $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJECTS)

# Rule to link the CLIENT executable
compiled/client: $(CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJECTS)

# --- COMPILATION RULES FOR EACH FILE INDIVIDUALLY ---
# This removes all ambiguity for the make utility.

compiled/source/main.o: source/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

compiled/source/server/server.o: source/server/server.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

compiled/source/core/filesystem.o: source/core/filesystem.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

compiled/source/core/queue.o: source/core/queue.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

compiled/source/core/hash_table.o: source/core/hash_table.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

compiled/source/client/client.o: source/client/client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# --- CLEAN RULE ---
clean:
	rm -rf compiled my_fs.omni