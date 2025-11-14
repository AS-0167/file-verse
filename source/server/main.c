
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "ofs_types.h"

// Forward declarations
int fs_format(const char* omni_path, const char* config_path);
int fs_init(void** instance, const char* omni_path, const char* config_path);
void fs_shutdown(void* instance);
int start_socket_server(void* instance, int port);

static void* g_instance = NULL;

void signal_handler(int sig) {
    printf("\nShutting down gracefully...\n");
    if (g_instance) {
        fs_shutdown(g_instance);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s format <omni_path> <config_path>  - Create new file system\n", argv[0]);
        printf("  %s <omni_path> <config_path>         - Start server\n", argv[0]);
        return 1;
    }
    
    // Handle format command
    if (strcmp(argv[1], "format") == 0) {
        if (argc != 4) {
            printf("Usage: %s format <omni_path> <config_path>\n", argv[0]);
            return 1;
        }
        
        printf("Formatting %s...\n", argv[2]);
        int result = fs_format(argv[2], argv[3]);
        
        if (result == OFS_SUCCESS) {
            printf("File system created successfully!\n");
            return 0;
        } else {
            printf("Failed to create file system: %d\n", result);
            return 1;
        }
    }
    
    // Normal startup
    if (argc != 3) {
        printf("Usage: %s <omni_path> <config_path>\n", argv[0]);
        return 1;
    }
    
    const char* omni_path = argv[1];
    const char* config_path = argv[2];
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize file system
    printf("Loading file system from %s...\n", omni_path);
    int result = fs_init(&g_instance, omni_path, config_path);
    
    if (result != OFS_SUCCESS) {
        printf("Failed to initialize: %d\n", result);
        return 1;
    }
    
    printf("File system loaded successfully!\n");
    printf("Starting server on port 8080...\n");
    
    // Start socket server (this blocks)
    result = start_socket_server(g_instance, 8080);
    
    // Cleanup
    fs_shutdown(g_instance);
    
    return 0;
}