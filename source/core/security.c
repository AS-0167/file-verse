#include "security.h"
#include <stdio.h>

// The one and only implementation of our password hashing.
void hash_password(const char* password, char* hash) {
    unsigned long h = 5381;
    int c;
    const char* str = password;
    
    // Make sure we are hashing a valid string
    if (!password || !hash) return;
    
    while ((c = *str++)) {
        h = ((h << 5) + h) + c; // h * 33 + c
    }
    
    // Securely format the string into the provided buffer
    snprintf(hash, 64, "%016lx", h);
}