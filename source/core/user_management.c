#include "ofs_types.h"
#include "hash_table.h"
#include "security.h" // Assuming security.h contains hash_password
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper function to free SessionInfo values, useful for ht_destroy
void free_session_info(void* session) {
    if (session) {
        free(session);
    }
}

int user_login(void* instance, const char* username, const char* password, char** session_id) {
    OFSInstance* ofs = (OFSInstance*)instance;

    if (!ofs || !username || !password || !session_id) {
        return OFS_ERROR_INVALID_PARAM;
    }

    UserInfo* user = (UserInfo*)ht_get(ofs->users, username);
    if (!user) {
        return OFS_ERROR_AUTH_FAILED;
    }

    char provided_hash[64];
    hash_password(password, provided_hash);
    if (strcmp(user->password_hash, provided_hash) != 0) {
        return OFS_ERROR_AUTH_FAILED;
    }

    SessionInfo* session = (SessionInfo*)calloc(1, sizeof(SessionInfo));
    if (!session) {
        return OFS_ERROR_SYSTEM;
    }

    snprintf(session->session_id, 64, "%s_%ld", username, time(NULL));
    session->user_id = user->user_id;
    strncpy(session->username, user->username, 31);
    session->role = user->role;
    session->login_time = time(NULL);
    session->is_valid = 1;

    int result = ht_insert(ofs->sessions, session->session_id, session);
    if (result != 0) {
        // If insertion fails, we must clean up the allocated session memory
        free(session);
        return OFS_ERROR_SYSTEM;
    }

    // The key for the hash table is now managed internally by ht_insert's strdup.
    // We still need to strdup the session_id to return it to the caller safely.
    *session_id = strdup(session->session_id);
    if (!(*session_id)) {
        // If strdup fails, we should ideally remove the session from the hash table
        ht_remove(ofs->sessions, session->session_id);
        return OFS_ERROR_SYSTEM;
    }

    return OFS_SUCCESS;
}

// --- NEW FUNCTION ADDED HERE ---
// ============================================================================
// user_logout - Invalidates a user session
// ============================================================================
int user_logout(SessionInfo* session) {
    // If the session pointer is NULL, it means the session ID was not found
    // in the hash table. From a user's perspective, they are already logged out,
    // so we can safely return success.
    if (!session) {
        return OFS_SUCCESS;
    }

    // This is the most important part of logging out:
    // We mark the session as invalid. The main server loop will now reject
    // any future requests that try to use this session.
    session->is_valid = 0; // Set to 0 for false/invalid

    // Optional: Add a log message for the server console
    printf("--- SESSION: Session for user_id %u has been invalidated (logged out). ---\n", session->user_id);

    return OFS_SUCCESS;
}





// ============================================================================
// user_create - Creates a new user account (Admin only)
// ============================================================================
int user_create(OFSInstance* ofs, SessionInfo* admin_session, const char* username, const char* password, UserRole role) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !admin_session || !username || !password) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- PERMISSION CHECK ---
    // Check if the user trying to create a new account is an Administrator.
    if (admin_session->role != ROLE_ADMIN) {
        printf("--- SECURITY: Non-admin user (ID: %u) attempted to create a new user. Denied. ---\n", admin_session->user_id);
        return OFS_ERROR_PERMISSION_DENIED;
    }

    // 3. --- CHECK IF USER ALREADY EXISTS ---
    if (ht_get(ofs->users, username) != NULL) {
        return OFS_ERROR_ALREADY_EXISTS;
    }
    
    // 4. --- CREATE THE NEW USER STRUCT ---
    UserInfo* new_user = (UserInfo*)calloc(1, sizeof(UserInfo));
    if (!new_user) {
        return OFS_ERROR_SYSTEM;
    }
    
    // For this simple system, we can use the next available entry index as the user ID.
    // A more complex system might have a separate user ID counter.
    new_user->user_id = ofs->next_entry_index; 
    strncpy(new_user->username, username, 31);
    new_user->username[31] = '\0';
    new_user->role = role;
    
    // Hash the new user's password
    hash_password(password, new_user->password_hash);
    
    // 5. --- ADD TO USERS HASH TABLE ---
    if (ht_insert(ofs->users, new_user->username, new_user) != 0) {
        free(new_user); // Clean up if the insertion fails
        return OFS_ERROR_SYSTEM;
    }
    
    // NOTE: We are not writing the user list to disk in this implementation.
    // The new user only exists in memory and will be gone when the server restarts.
    // A persistent user system would require writing to a user file.

    printf("--- USER MGMT: Admin (ID: %u) created new user '%s' (ID: %u). ---\n", admin_session->user_id, new_user->username, new_user->user_id);
    
    // We can increment next_entry_index to ensure the next user/file gets a unique ID.
    ofs->next_entry_index++;

    return OFS_SUCCESS;
}




// ============================================================================
// user_delete - Deletes a user account (Admin only)
// ============================================================================
int user_delete(OFSInstance* ofs, SessionInfo* admin_session, const char* username) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !admin_session || !username) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- PERMISSION CHECK ---
    if (admin_session->role != ROLE_ADMIN) {
        printf("--- SECURITY: Non-admin user (ID: %u) attempted to delete a user. Denied. ---\n", admin_session->user_id);
        return OFS_ERROR_PERMISSION_DENIED;
    }

    // 3. --- PREVENT ADMIN FROM DELETING THEMSELVES ---
    // It's good practice to prevent the admin from deleting their own account via this function.
    if (strcmp(admin_session->username, username) == 0) {
        return OFS_ERROR_PERMISSION_DENIED; // Cannot delete yourself
    }

    // 4. --- FIND AND REMOVE THE USER ---
    // We need to first get the user info so we can free its memory.
    UserInfo* user_to_delete = (UserInfo*)ht_get(ofs->users, username);
    if (!user_to_delete) {
        return OFS_ERROR_NOT_FOUND; // User does not exist
    }

    // Now, remove the user from the hash table.
    // The ht_remove function should handle unlinking the node.
    if (ht_remove(ofs->users, username) != 0) {
        // This would indicate a problem with the hash table itself.
        return OFS_ERROR_SYSTEM;
    }

    // 5. --- FREE THE USERINFO STRUCT ---
    // After removing the user from the table, we must free the memory
    // that was allocated for the UserInfo struct to prevent a memory leak.
    free(user_to_delete);
    
    // NOTE: Like user_create, this only affects the in-memory store.
    // The user will reappear if the server is restarted.

    printf("--- USER MGMT: Admin (ID: %u) deleted user '%s'. ---\n", admin_session->user_id, username);

    return OFS_SUCCESS;
}


// ============================================================================
// user_list - Retrieves a list of all users (Admin only)
// ============================================================================
int user_list(OFSInstance* ofs, SessionInfo* admin_session, UserInfo** users_out, int* count_out) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !admin_session || !users_out || !count_out) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- PERMISSION CHECK ---
    if (admin_session->role != ROLE_ADMIN) {
        return OFS_ERROR_PERMISSION_DENIED;
    }

    // 3. --- RETRIEVE ALL VALUES FROM THE USERS HASH TABLE ---
    size_t user_count = ofs->users->size;
    *count_out = (int)user_count;

    if (user_count == 0) {
        *users_out = NULL;
        return OFS_SUCCESS;
    }

    // 4. --- ALLOCATE MEMORY FOR THE OUTPUT ARRAY ---
    // We are creating a copy of the user data to return to the caller.
    // The caller will be responsible for freeing this memory.
    *users_out = (UserInfo*)malloc(user_count * sizeof(UserInfo));
    if (!(*users_out)) {
        *count_out = 0;
        return OFS_ERROR_SYSTEM; // Out of memory
    }

    // 5. --- ITERATE AND COPY USER DATA ---
    // To get all values, we iterate through the hash table's buckets.
    int current_index = 0;
    for (size_t i = 0; i < ofs->users->capacity; i++) {
        HashNode* node = ofs->users->buckets[i];
        while (node) {
            if (current_index < user_count) {
                // Copy the UserInfo struct from the hash table into our output array.
                // We do a direct memory copy.
                memcpy(&((*users_out)[current_index]), node->value, sizeof(UserInfo));
            }
            current_index++;
            node = node->next;
        }
    }

    return OFS_SUCCESS;
}



// ============================================================================
// get_session_info - Retrieves details about the current session
// ============================================================================
int get_session_info(SessionInfo* session, SessionInfo* info_out) {
    // 1. --- VALIDATE INPUTS ---
    if (!session || !info_out) {
        return OFS_ERROR_INVALID_PARAM;
    }
    
    // 2. --- CHECK IF SESSION IS VALID ---
    // The main server loop already does this, but it's good practice for the function
    // to be self-contained and safe.
    if (!session->is_valid) {
        return OFS_ERROR_SESSION_INVALID;
    }

    // 3. --- COPY DATA ---
    // Copy the contents of the session struct to the output struct.
    // A direct memory copy is the most efficient way to do this.
    memcpy(info_out, session, sizeof(SessionInfo));

    return OFS_SUCCESS;
}


// ============================================================================
// get_error_message - Translates an OFSErrorCode into a human-readable string
// ============================================================================
const char* get_error_message(int error_code) {
    // We use a switch statement to map the enum values to strings.
    switch (error_code) {
        case OFS_SUCCESS:
            return "Operation successful.";
        case OFS_ERROR_INVALID_PARAM:
            return "Invalid parameter provided.";
        case OFS_ERROR_PERMISSION_DENIED:
            return "Permission denied.";
        case OFS_ERROR_NOT_FOUND:
            return "File or resource not found.";
        case OFS_ERROR_ALREADY_EXISTS:
            return "File or resource already exists.";
        case OFS_ERROR_NOT_EMPTY:
            return "Directory is not empty.";
        case OFS_ERROR_NO_SPACE:
            return "Not enough space.";
        case OFS_ERROR_IO:
            return "Input/output error.";
        case OFS_ERROR_CORRUPTED:
            return "File system is corrupted.";
        case OFS_ERROR_AUTH_FAILED:
            return "Authentication failed.";
        case OFS_ERROR_SESSION_INVALID:
            return "Session is invalid or has expired.";
        case OFS_ERROR_NOT_DIRECTORY:
            return "The specified path is not a directory.";
        case OFS_ERROR_NOT_A_FILE:
            return "The specified path is not a file.";
        case OFS_ERROR_FILE_TOO_LARGE:
            return "File is too large.";
        case OFS_ERROR_SYSTEM:
            return "An unexpected system error occurred.";
        
        // Add any other custom error codes you have defined here.

        default:
            return "An unknown error occurred.";
    }
}