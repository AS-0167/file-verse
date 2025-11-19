#include <iostream>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <string>
#include "include/ofs_core.hpp"
#include "server/ofs_server.hpp"

// Global instances for signal handling
OFSServer* g_server = nullptr;
OFSCore* g_ofs_core = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    
    if (g_server) {
        g_server->stop();
    }
    
    if (g_ofs_core) {
        std::cout << "Shutting down file system...\n";
        g_ofs_core->shutdown();
    }
    
    exit(signum);
}

// Simple configuration parser
class ConfigParser {
public:
    static int getInt(const std::string& content, const std::string& key, int defaultVal) {
        size_t pos = content.find(key);
        if (pos == std::string::npos) return defaultVal;
        
        pos = content.find("=", pos);
        if (pos == std::string::npos) return defaultVal;
        
        // Skip whitespace
        pos++;
        while (pos < content.length() && (content[pos] == ' ' || content[pos] == '\t')) {
            pos++;
        }
        
        if (pos >= content.length()) return defaultVal;
        
        return std::atoi(content.c_str() + pos);
    }
    
    static std::string readFile(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        
        file.close();
        return content;
    }
};

bool fileExists(const char* path) {
    std::ifstream file(path);
    return file.good();
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   OFS Server - Phase 1                \n";
    std::cout << "   Student: BSCS24115                  \n";
    std::cout << "========================================\n\n";
    
    // Parse command line arguments
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <omni_file> <config_file> [--format]\n";
        std::cerr << "\nExamples:\n";
        std::cerr << "  " << argv[0] << " BSCS24115.omni compiled/default.uconf --format\n";
        std::cerr << "  " << argv[0] << " BSCS24115.omni compiled/default.uconf\n";
        return 1;
    }
    
    const char* omni_path = argv[1];
    const char* config_path = argv[2];
    bool should_format = (argc > 3 && std::string(argv[3]) == "--format");
    
    std::cout << "Configuration:\n";
    std::cout << "  OMNI File: " << omni_path << "\n";
    std::cout << "  Config File: " << config_path << "\n";
    std::cout << "  Mode: " << (should_format ? "FORMAT" : "LOAD") << "\n\n";
    
    // Read configuration for port
    std::string config_content = ConfigParser::readFile(config_path);
    if (config_content.empty()) {
        std::cerr << "Error: Could not read config file: " << config_path << "\n";
        return 1;
    }
    
    int port = ConfigParser::getInt(config_content, "port", 8080);
    
    // Create OFSCore instance
    g_ofs_core = new OFSCore();
    
    // Format or Initialize
    if (should_format || !fileExists(omni_path)) {
        std::cout << "=== Formatting File System ===\n";
        int result = g_ofs_core->format(omni_path, config_path);
        
        if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
            std::cerr << "Error: Failed to format file system: " 
                      << g_ofs_core->getErrorMessage(result) << "\n";
            delete g_ofs_core;
            return 1;
        }
        
        std::cout << "✓ File system formatted successfully\n\n";
    }
    
    // Initialize file system
    std::cout << "=== Initializing File System ===\n";
    int result = g_ofs_core->initialize(omni_path, config_path);
    
    if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cerr << "Error: Failed to initialize file system: " 
                  << g_ofs_core->getErrorMessage(result) << "\n";
        delete g_ofs_core;
        return 1;
    }
    
    std::cout << "✓ File system initialized successfully\n\n";
    
    // Create and initialize server
    std::cout << "=== Initializing Server ===\n";
    OFSServer server(port);
    g_server = &server;
    
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (!server.initialize(g_ofs_core)) {
        std::cerr << "Error: Failed to initialize server\n";
        g_ofs_core->shutdown();
        delete g_ofs_core;
        return 1;
    }
    
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   SERVER READY                        \n";
    std::cout << "========================================\n";
    std::cout << "Listening on port: " << port << "\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    std::cout << "Default credentials:\n";
    std::cout << "  Username: admin\n";
    std::cout << "  Password: admin123\n";
    std::cout << "========================================\n\n";
    
    // Run server (blocking)
    server.run();
    
    // Cleanup
    std::cout << "\n=== Shutting Down ===\n";
    g_ofs_core->shutdown();
    delete g_ofs_core;
    
    std::cout << "Goodbye!\n";
    return 0;
}