#include "ofs_types.h"
#include "hash_table.h"
#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int user_login(void* instance, const char* username, const char* password, char** session_id) {
    OFSInstance* ofs = (OFSInstance*)instance;

    if (!ofs || !username || !password || !session_id) {
        return OFS_ERROR_INVALID_PARAM;
    }

    UserInfo* user = (UserInfo*)ht_get(ofs->users, username);
    if (!user || !user->is_active) {
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

    if (ht_insert(ofs->sessions, session->session_id, session) != 0) {
        free(session);
        return OFS_ERROR_SYSTEM;
    }

    *session_id = strdup(session->session_id);
    if (!(*session_id)) {
        ht_remove(ofs->sessions, session->session_id);
        return OFS_ERROR_SYSTEM;
    }

    return OFS_SUCCESS;
}

int user_logout(SessionInfo* session) {
    if (!session) {
        return OFS_SUCCESS;
    }
    session->is_valid = 0;
    return OFS_SUCCESS;
}

int user_create(OFSInstance* ofs, SessionInfo* admin_session, const char* username, const char* password, UserRole role) {
    if (!ofs || !admin_session || !username || !password) {
        return OFS_ERROR_INVALID_PARAM;
    }
    if (admin_session->role != ROLE_ADMIN) {
        return OFS_ERROR_PERMISSION_DENIED;
    }
    if (ht_get(ofs->users, username) != NULL) {
        return OFS_ERROR_ALREADY_EXISTS;
    }
    
    int user_slot = -1;
    for (uint32_t i = 0; i < ofs->header.max_users; i++) {
        UserInfo temp_user;
        fseek(ofs->omni_file, ofs->header.user_table_offset + (i * sizeof(UserInfo)), SEEK_SET);
        fread(&temp_user, sizeof(UserInfo), 1, ofs->omni_file);
        if (temp_user.is_active == 0) {
            user_slot = i;
            break;
        }
    }

    if (user_slot == -1) {
        return OFS_ERROR_NO_SPACE;
    }

    UserInfo* new_user = (UserInfo*)calloc(1, sizeof(UserInfo));
    if (!new_user) return OFS_ERROR_SYSTEM;
    
    new_user->user_id = user_slot;
    new_user->is_active = 1;
    new_user->role = role;
    // --- THIS IS THE FIX: The line below has been removed ---
    // new_user->created_time = time(NULL); // This line caused the error and is now gone.
    strncpy(new_user->username, username, 31);
    hash_password(password, new_user->password_hash);
    
    fseek(ofs->omni_file, ofs->header.user_table_offset + (user_slot * sizeof(UserInfo)), SEEK_SET);
    fwrite(new_user, sizeof(UserInfo), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    if (ht_insert(ofs->users, new_user->username, new_user) != 0) {
        free(new_user);
        return OFS_ERROR_SYSTEM;
    }

    return OFS_SUCCESS;
}

int user_delete(OFSInstance* ofs, SessionInfo* admin_session, const char* username) {
    if (!ofs || !admin_session || !username) {
        return OFS_ERROR_INVALID_PARAM;
    }
    if (admin_session->role != ROLE_ADMIN) {
        return OFS_ERROR_PERMISSION_DENIED;
    }
    if (strcmp(admin_session->username, username) == 0) {
        return OFS_ERROR_PERMISSION_DENIED;
    }

    UserInfo* user_to_delete = (UserInfo*)ht_get(ofs->users, username);
    if (!user_to_delete) {
        return OFS_ERROR_NOT_FOUND;
    }

    uint32_t user_id = user_to_delete->user_id;

    user_to_delete->is_active = 0;
    fseek(ofs->omni_file, ofs->header.user_table_offset + (user_id * sizeof(UserInfo)), SEEK_SET);
    fwrite(user_to_delete, sizeof(UserInfo), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    ht_remove(ofs->users, username);
    free(user_to_delete);

    return OFS_SUCCESS;
}

int user_list(OFSInstance* ofs, SessionInfo* admin_session, UserInfo** users_out, int* count_out) {
    if (!ofs || !admin_session || !users_out || !count_out) {
        return OFS_ERROR_INVALID_PARAM;
    }
    if (admin_session->role != ROLE_ADMIN) {
        return OFS_ERROR_PERMISSION_DENIED;
    }

    size_t user_count = ofs->users->size;
    *count_out = (int)user_count;

    if (user_count == 0) {
        *users_out = NULL;
        return OFS_SUCCESS;
    }

    *users_out = (UserInfo*)malloc(user_count * sizeof(UserInfo));
    if (!(*users_out)) {
        *count_out = 0;
        return OFS_ERROR_SYSTEM;
    }

    int current_index = 0;
    for (size_t i = 0; i < ofs->users->capacity; i++) {
        HashNode* node = ofs->users->buckets[i];
        while (node) {
            if (current_index < user_count) {
                memcpy(&((*users_out)[current_index]), node->value, sizeof(UserInfo));
            }
            current_index++;
            node = node->next;
        }
    }

    return OFS_SUCCESS;
}

int get_session_info(SessionInfo* session, SessionInfo* info_out) {
    if (!session || !info_out) {
        return OFS_ERROR_INVALID_PARAM;
    }
    if (!session->is_valid) {
        return OFS_ERROR_SESSION_INVALID;
    }
    memcpy(info_out, session, sizeof(SessionInfo));
    return OFS_SUCCESS;
}

const char* get_error_message(int error_code) {
    switch (error_code) {
        case OFS_SUCCESS: return "Operation successful.";
        case OFS_ERROR_INVALID_PARAM: return "Invalid parameter provided.";
        case OFS_ERROR_PERMISSION_DENIED: return "Permission denied.";
        case OFS_ERROR_NOT_FOUND: return "File or resource not found.";
        case OFS_ERROR_ALREADY_EXISTS: return "File or resource already exists.";
        case OFS_ERROR_NOT_EMPTY: return "Directory is not empty.";
        case OFS_ERROR_NO_SPACE: return "Not enough space.";
        case OFS_ERROR_IO: return "Input/output error.";
        case OFS_ERROR_CORRUPTED: return "File system is corrupted.";
        case OFS_ERROR_AUTH_FAILED: return "Authentication failed.";
        case OFS_ERROR_SESSION_INVALID: return "Session is invalid or has expired.";
        case OFS_ERROR_NOT_DIRECTORY: return "The specified path is not a directory.";
        case OFS_ERROR_NOT_A_FILE: return "The specified path is not a file.";
        case OFS_ERROR_FILE_TOO_LARGE: return "File is too large.";
        case OFS_ERROR_SYSTEM: return "An unexpected system error occurred.";
        default: return "An unknown error occurred.";
    }
}