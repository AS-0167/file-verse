#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ofs_types.h"

// Forward declaration for the formatting function
int fs_format(const char* omni_path, const char* config_path);

// This is the main function for the temporary 'ofs_format_tool'
int main(int argc, char* argv[]) {
    if (argc != 4 || strcmp(argv[1], "format") != 0) {
        printf("Usage: %s format <omni_path> <config_path>\n", argv[0]);
        return 1;
    }
    
    const char* omni_path = argv[2];
    const char* config_path = argv[3];
    
    printf("Formatting %s using config %s...\n", omni_path, config_path);
    int result = fs_format(omni_path, config_path);
    
    if (result == OFS_SUCCESS) {
        printf("File system created successfully!\n");
        return 0;
    } else {
        printf("Failed to create file system: error code %d\n", result);
        return 1;
    }
}