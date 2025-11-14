#ifndef SECURITY_H
#define SECURITY_H

// This is the single, official function for hashing passwords.
void hash_password(const char* password, char* hash_buffer_64_bytes);

#endif // SECURITY_H