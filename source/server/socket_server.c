// THIS IS THE FINAL, FULLY IMPLEMENTED, AND CORRECT version of socket_server.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <json-c/json.h>

#include "ofs_types.h"
#include "queue.h"
#include "security.h"
#include "hash_table.h"

// --- FORWARD DECLARATIONS ---
int user_login(OFSInstance* instance, const char* username, const char* password, char** session_id);
int dir_list(OFSInstance* ofs, SessionInfo* session, const char* path, FileEntry** entries_out, int* count_out);
int dir_create(OFSInstance* ofs, SessionInfo* session, const char* path);
int dir_exists(OFSInstance* ofs, SessionInfo* session, const char* path);
int file_create(OFSInstance* ofs, SessionInfo* session, const char* path, const char* data, size_t size);
int file_read(OFSInstance* ofs, SessionInfo* session, const char* path, char** buffer_out, size_t* size_out);
int file_delete(OFSInstance* ofs, SessionInfo* session, const char* path);
int dir_delete(OFSInstance* ofs, SessionInfo* session, const char* path);
int file_edit(OFSInstance* ofs, SessionInfo* session, const char* path, const char* data, size_t size, uint32_t index);
int file_rename(OFSInstance* ofs, SessionInfo* session, const char* old_path, const char* new_path);
int user_logout(SessionInfo* session);
int user_create(OFSInstance* ofs, SessionInfo* admin_session, const char* username, const char* password, UserRole role);
int user_delete(OFSInstance* ofs, SessionInfo* admin_session, const char* username);
int user_list(OFSInstance* ofs, SessionInfo* admin_session, UserInfo** users_out, int* count_out);
int get_session_info(SessionInfo* session, SessionInfo* info_out);
int get_metadata(OFSInstance* ofs, SessionInfo* session, const char* path, FileEntry* meta_out);
int set_permissions(OFSInstance* ofs, SessionInfo* session, const char* path, uint32_t permissions);
int get_stats(OFSInstance* ofs, SessionInfo* session, FSStats* stats_out);
const char* get_error_message(int error_code);
int file_truncate(OFSInstance* ofs, SessionInfo* session, const char* path);






const char* find_json_body(const char* http_request) {
    const char* body_start = strstr(http_request, "\r\n\r\n");
    if (body_start) { return body_start + 4; }
    return http_request;
}

Queue* request_queue;
typedef struct { int client_socket; char* request_json; } ClientRequest;

void* worker_thread_function(void* arg) {
    OFSInstance* ofs_instance = (OFSInstance*)arg;

    while (1) {
        ClientRequest* req = (ClientRequest*)queue_dequeue(request_queue);
        if (!req) continue;
        
        struct json_object *response_json = json_object_new_object();
        json_object_object_add(response_json, "status", json_object_new_string("error"));
        
        const char* json_body = find_json_body(req->request_json);
        struct json_object *parsed_json = json_tokener_parse(json_body);
        
        if (!parsed_json) {
            json_object_object_add(response_json, "error_message", json_object_new_string("Invalid JSON format"));
        } else {
            struct json_object *operation_obj, *params_obj, *session_obj;
            json_object_object_get_ex(parsed_json, "operation", &operation_obj);
            json_object_object_get_ex(parsed_json, "parameters", &params_obj);
            json_object_object_get_ex(parsed_json, "session_id", &session_obj);

            if (!operation_obj) {
                json_object_object_add(response_json, "error_message", json_object_new_string("Missing 'operation' field"));
            } else {
                const char* operation = json_object_get_string(operation_obj);
                
                if (strcmp(operation, "user_login") == 0) {
                    struct json_object *user_obj, *pass_obj;
                    json_object_object_get_ex(params_obj, "username", &user_obj);
                    json_object_object_get_ex(params_obj, "password", &pass_obj);
                    if (!user_obj || !pass_obj) {
                        json_object_object_add(response_json, "error_message", json_object_new_string("Missing username or password"));
                    } else {
                        const char* username = json_object_get_string(user_obj);
                        const char* password = json_object_get_string(pass_obj);
                        char* session_id = NULL;
                        int result = user_login(ofs_instance, username, password, &session_id);
                        if (result == OFS_SUCCESS) {
                            json_object_object_add(response_json, "status", json_object_new_string("success"));
                            struct json_object* data_obj = json_object_new_object();
                            json_object_object_add(data_obj, "session_id", json_object_new_string(session_id));
                            json_object_object_add(response_json, "data", data_obj);
                            free(session_id);
                        } else {
                            json_object_object_add(response_json, "error_message", json_object_new_string("Login failed"));
                        }
                    }
                }

                 else if (strcmp(operation, "user_logout") == 0) {
                    struct json_object *session_obj;
                    // For logout, the session_id is a top-level field
                    json_object_object_get_ex(parsed_json, "session_id", &session_obj);

                    if (!session_obj) {
                        json_object_object_add(response_json, "error_message", json_object_new_string("Missing session_id"));
                    } else {
                        const char* session_id_str = json_object_get_string(session_obj);
                        SessionInfo* session = (SessionInfo*)ht_get(ofs_instance->sessions, session_id_str);

                        // Call the new logout function from user_management.c
                        int result = user_logout(session);

                        if (result == OFS_SUCCESS) {
                            json_object_object_add(response_json, "status", json_object_new_string("success"));
                        } else {
                            json_object_object_add(response_json, "error_message", json_object_new_string("Logout failed"));
                        }
                    }
                }




                 else if (strcmp(operation, "get_error_message") == 0) {
                    struct json_object *code_obj;
                    json_object_object_get_ex(params_obj, "error_code", &code_obj);

                    if (!code_obj) {
                        json_object_object_add(response_json, "error_message", json_object_new_string("Missing error_code parameter"));
                    } else {
                        int error_code = json_object_get_int(code_obj);
                        const char* message = get_error_message(error_code);

                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                        struct json_object* data_obj = json_object_new_object();
                        json_object_object_add(data_obj, "message", json_object_new_string(message));
                        json_object_object_add(response_json, "data", data_obj);
                    }
                }
                
                
                
                
                else {
                    if (!session_obj) {
                        json_object_object_add(response_json, "error_message", json_object_new_string("Missing session_id"));
                    } else {
                        const char* session_id_str = json_object_get_string(session_obj);
                        SessionInfo* session = (SessionInfo*)ht_get(ofs_instance->sessions, session_id_str);

                        if (!session || !session->is_valid) {
                            json_object_object_add(response_json, "error_message", json_object_new_string("Invalid session"));
                        } else {
                            // --- SESSION IS VALID, PROCEED ---
                            if (strcmp(operation, "dir_list") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    FileEntry* entries = NULL;
                                    int count = 0;
                                    int result = dir_list(ofs_instance, session, path, &entries, &count);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                        struct json_object* data_obj = json_object_new_object();
                                        struct json_object* arr = json_object_new_array();
                                        for(int i=0; i<count; i++){
                                            struct json_object* entry_obj = json_object_new_object();
                                            json_object_object_add(entry_obj, "name", json_object_new_string(entries[i].name));
                                            json_object_object_add(entry_obj, "is_directory", json_object_new_boolean(entries[i].is_directory));
                                            json_object_object_add(entry_obj, "size", json_object_new_int64(entries[i].total_size));
                                            json_object_array_add(arr, entry_obj);
                                        }
                                        json_object_object_add(data_obj, "entries", arr);
                                        json_object_object_add(response_json, "data", data_obj);
                                        if(entries) free(entries);
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to list dir"));
                                    }
                                }
                            }



                            else if (strcmp(operation, "dir_create") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    int result = dir_create(ofs_instance, session, path);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to create dir"));
                                    }
                                }
                            }



                            else if (strcmp(operation, "file_create") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    const char* data = ""; 
                                    size_t size = 0;
                                    struct json_object *data_param_obj;
                                    json_object_object_get_ex(params_obj, "data", &data_param_obj);
                                    if (data_param_obj) {
                                        data = json_object_get_string(data_param_obj);
                                        size = strlen(data);
                                    }
                                    int result = file_create(ofs_instance, session, path, data, size);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to create file"));
                                    }
                                }
                            }



                            else if (strcmp(operation, "file_read") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    char* buffer = NULL;
                                    size_t size = 0;
                                    int result = file_read(ofs_instance, session, path, &buffer, &size);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                        struct json_object* data_obj = json_object_new_object();
                                       json_object_object_add(data_obj, "content", json_object_new_string_len(buffer ? buffer : "", size));
                                        json_object_object_add(response_json, "data", data_obj);
                                        if (buffer) free(buffer);
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to read file"));
                                    }
                                }
                            }



                            else if (strcmp(operation, "file_delete") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    int result = file_delete(ofs_instance, session, path);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to delete file"));
                                    }
                                }
                            }


                             else if (strcmp(operation, "dir_delete") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    int result = dir_delete(ofs_instance, session, path);
                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        // Your dir_delete returns OFS_ERROR_NOT_EMPTY, so we can make this message more specific.
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to delete directory. It may not exist or is not empty."));
                                    }
                                }
                            }


                            else if (strcmp(operation, "dir_exists") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);

                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    int result = dir_exists(ofs_instance, session, path);

                                    // This API call always succeeds and returns the answer in the 'data' field.
                                    json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    struct json_object* data_obj = json_object_new_object();
                                    
                                    // The 'exists' field is true only if the function returned OFS_SUCCESS.
                                    json_object_object_add(data_obj, "exists", json_object_new_boolean(result == OFS_SUCCESS));
                                    
                                    json_object_object_add(response_json, "data", data_obj);
                                }
                            }



                              else if (strcmp(operation, "file_edit") == 0) {
                                struct json_object *path_obj, *data_obj, *index_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                json_object_object_get_ex(params_obj, "data", &data_obj);
                                json_object_object_get_ex(params_obj, "index", &index_obj);

                                if (!path_obj || !data_obj || !index_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path, data, or index parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    const char* data = json_object_get_string(data_obj);
                                    uint32_t index = json_object_get_int(index_obj);
                                    size_t size = strlen(data);

                                    int result = file_edit(ofs_instance, session, path, data, size, index);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to edit file"));
                                    }
                                }
                            }
                           




                            else if (strcmp(operation, "file_edit") == 0) {
                                // ... your existing file_edit code ...
                            }
                            
                            // --- PASTE THE NEW BLOCK BELOW ---
                            else if (strcmp(operation, "file_rename") == 0) {
                                struct json_object *old_path_obj, *new_path_obj;
                                json_object_object_get_ex(params_obj, "old_path", &old_path_obj);
                                json_object_object_get_ex(params_obj, "new_path", &new_path_obj);

                                if (!old_path_obj || !new_path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing old_path or new_path parameter"));
                                } else {
                                    const char* old_path = json_object_get_string(old_path_obj);
                                    const char* new_path = json_object_get_string(new_path_obj);

                                    // This is the function you will write next
                                    int result = file_rename(ofs_instance, session, old_path, new_path);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to rename or move file"));
                                    }
                                }
                            }


                             else if (strcmp(operation, "user_create") == 0) {
                                struct json_object *user_obj, *pass_obj, *role_obj;
                                json_object_object_get_ex(params_obj, "username", &user_obj);
                                json_object_object_get_ex(params_obj, "password", &pass_obj);
                                json_object_object_get_ex(params_obj, "role", &role_obj);

                                if (!user_obj || !pass_obj || !role_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing username, password, or role parameter"));
                                } else {
                                    const char* username = json_object_get_string(user_obj);
                                    const char* password = json_object_get_string(pass_obj);
                                    int role_int = json_object_get_int(role_obj);
                                    
                                    // --- CORRECTED MAPPING LOGIC ---
                                    UserRole role;
                                    if (role_int == 0) {
                                        role = ROLE_ADMIN;
                                    } else if (role_int == 2) {
                                        role = ROLE_GUEST;
                                    } else {
                                        role = ROLE_USER; // Default to ROLE_USER for 1 or any other number
                                    }
                                    // --- END OF CORRECTION ---

                                    // 'session' here is the admin's session, passed to the function
                                    int result = user_create(ofs_instance, session, username, password, role);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else if (result == OFS_ERROR_PERMISSION_DENIED) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Permission denied"));
                                    } else if (result == OFS_ERROR_ALREADY_EXISTS) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("User already exists"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to create user"));
                                    }
                                }
                            }



                             else if (strcmp(operation, "user_delete") == 0) {
                                struct json_object *user_obj;
                                json_object_object_get_ex(params_obj, "username", &user_obj);

                                if (!user_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing username parameter"));
                                } else {
                                    const char* username = json_object_get_string(user_obj);

                                    // 'session' here is the admin's session
                                    int result = user_delete(ofs_instance, session, username);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else if (result == OFS_ERROR_PERMISSION_DENIED) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Permission denied"));
                                    } else if (result == OFS_ERROR_NOT_FOUND) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("User not found"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to delete user"));
                                    }
                                }
                            }


                            else if (strcmp(operation, "user_list") == 0) {
                                UserInfo* users = NULL;
                                int count = 0;
                                // 'session' here is the admin's session
                                int result = user_list(ofs_instance, session, &users, &count);

                                if (result == OFS_SUCCESS) {
                                    json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    
                                    struct json_object* data_obj = json_object_new_object();
                                    struct json_object* users_array = json_object_new_array();

                                    // Loop through the C array of users and build a JSON array
                                    for (int i = 0; i < count; i++) {
                                        struct json_object* user_obj = json_object_new_object();
                                        json_object_object_add(user_obj, "user_id", json_object_new_int(users[i].user_id));
                                        json_object_object_add(user_obj, "username", json_object_new_string(users[i].username));
                                        json_object_object_add(user_obj, "role", json_object_new_int(users[i].role));
                                        // We don't include the password hash in the response for security.
                                        
                                        json_object_array_add(users_array, user_obj);
                                    }
                                    
                                    json_object_object_add(data_obj, "users", users_array);
                                    json_object_object_add(response_json, "data", data_obj);

                                    // IMPORTANT: Free the memory that was allocated by the user_list function
                                    if (users) {
                                        free(users);
                                    }

                                } else if (result == OFS_ERROR_PERMISSION_DENIED) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Permission denied"));
                                } else {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Failed to retrieve user list"));
                                }
                            }



                              else if (strcmp(operation, "get_session_info") == 0) {
                                SessionInfo session_info_data;
                                
                                // 'session' is the currently validated session from the server loop
                                int result = get_session_info(session, &session_info_data);

                                if (result == OFS_SUCCESS) {
                                    json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    
                                    struct json_object* data_obj = json_object_new_object();
                                    
                                    // Build a JSON object from the SessionInfo struct
                                    json_object_object_add(data_obj, "session_id", json_object_new_string(session_info_data.session_id));
                                    json_object_object_add(data_obj, "user_id", json_object_new_int(session_info_data.user_id));
                                    json_object_object_add(data_obj, "username", json_object_new_string(session_info_data.username));
                                    json_object_object_add(data_obj, "role", json_object_new_int(session_info_data.role));
                                    json_object_object_add(data_obj, "login_time", json_object_new_int64(session_info_data.login_time));
                                    
                                    json_object_object_add(response_json, "data", data_obj);
                                } else {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Failed to get session info"));
                                }
                            }




                             else if (strcmp(operation, "get_metadata") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);

                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    FileEntry metadata; // Use FileEntry to hold the result

                                    int result = get_metadata(ofs_instance, session, path, &metadata);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                        
                                        struct json_object* data_obj = json_object_new_object();
                                        
                                        // Build a detailed JSON object from the metadata struct
                                        json_object_object_add(data_obj, "name", json_object_new_string(metadata.name));
                                        json_object_object_add(data_obj, "is_directory", json_object_new_boolean(metadata.is_directory));
                                        json_object_object_add(data_obj, "size", json_object_new_int64(metadata.total_size));
                                        json_object_object_add(data_obj, "owner_id", json_object_new_int(metadata.owner_id));
                                        json_object_object_add(data_obj, "permissions", json_object_new_int(metadata.permissions));
                                        json_object_object_add(data_obj, "created_time", json_object_new_int64(metadata.created_time));
                                        json_object_object_add(data_obj, "modified_time", json_object_new_int64(metadata.modified_time));
                                        json_object_object_add(data_obj, "entry_index", json_object_new_int(metadata.entry_index));

                                        json_object_object_add(response_json, "data", data_obj);
                                    } else if (result == OFS_ERROR_NOT_FOUND) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("File or directory not found"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to get metadata"));
                                    }
                                }
                            }


                              else if (strcmp(operation, "set_permissions") == 0) {
                                struct json_object *path_obj, *perms_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);
                                json_object_object_get_ex(params_obj, "permissions", &perms_obj);

                                if (!path_obj || !perms_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path or permissions parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    uint32_t permissions = json_object_get_int(perms_obj);

                                    int result = set_permissions(ofs_instance, session, path, permissions);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else if (result == OFS_ERROR_NOT_FOUND) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("File or directory not found"));
                                    } else if (result == OFS_ERROR_PERMISSION_DENIED) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Permission denied"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to set permissions"));
                                    }
                                }
                            }



                            else if (strcmp(operation, "get_stats") == 0) {
                                FSStats stats;
                                int result = get_stats(ofs_instance, session, &stats);

                                if (result == OFS_SUCCESS) {
                                    json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    
                                    struct json_object* data_obj = json_object_new_object();
                                    
                                    json_object_object_add(data_obj, "total_size", json_object_new_int64(stats.total_size));
                                    json_object_object_add(data_obj, "used_space", json_object_new_int64(stats.used_space));
                                    json_object_object_add(data_obj, "free_space", json_object_new_int64(stats.free_space));
                                    json_object_object_add(data_obj, "total_blocks", json_object_new_int(stats.total_blocks));
                                    json_object_object_add(data_obj, "used_blocks", json_object_new_int(stats.used_blocks));
                                    json_object_object_add(data_obj, "free_blocks", json_object_new_int(stats.free_blocks));
                                    json_object_object_add(data_obj, "total_files", json_object_new_int(stats.total_files));
                                    
                                    json_object_object_add(response_json, "data", data_obj);
                                } else {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Failed to get file system stats"));
                                }
                            }



                             else if (strcmp(operation, "file_truncate") == 0) {
                                struct json_object *path_obj;
                                json_object_object_get_ex(params_obj, "path", &path_obj);

                                if (!path_obj) {
                                    json_object_object_add(response_json, "error_message", json_object_new_string("Missing path parameter"));
                                } else {
                                    const char* path = json_object_get_string(path_obj);
                                    int result = file_truncate(ofs_instance, session, path);

                                    if (result == OFS_SUCCESS) {
                                        json_object_object_add(response_json, "status", json_object_new_string("success"));
                                    } else if (result == OFS_ERROR_NOT_FOUND) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("File not found"));
                                    } else if (result == OFS_ERROR_PERMISSION_DENIED) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Permission denied"));
                                    } else if (result == OFS_ERROR_IS_DIRECTORY) {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Cannot truncate a directory"));
                                    } else {
                                        json_object_object_add(response_json, "error_message", json_object_new_string("Failed to truncate file"));
                                    }
                                }
                            }
                    
                            










                            else {
                                json_object_object_add(response_json, "error_message", json_object_new_string("Unknown operation"));
                            }
                        }
                    }
                }
            }
        }
        
        const char* json_response_body = json_object_to_json_string(response_json);
        char http_response[4096];
        snprintf(http_response, sizeof(http_response), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n%s", strlen(json_response_body), json_response_body);
        
        send(req->client_socket, http_response, strlen(http_response), 0);
        close(req->client_socket);
        
        if (parsed_json) json_object_put(parsed_json);
        json_object_put(response_json);
        free(req->request_json);
        free(req);
    }
    return NULL;
}

// --- MAIN SERVER FUNCTION ---
int start_socket_server(void* instance, int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    request_queue = queue_create();

    pthread_t worker_thread;
    if (pthread_create(&worker_thread, NULL, worker_thread_function, instance) != 0) {
        perror("Failed to create worker thread");
        return -1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);

    while (1) {
        int client_socket;
        struct sockaddr_in client_address;
        int addrlen = sizeof(client_address);

        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        printf("New connection accepted from socket %d\n", client_socket);
        char buffer[4096] = {0};
        read(client_socket, buffer, 4096);
        
        ClientRequest* req = (ClientRequest*)malloc(sizeof(ClientRequest));
        req->client_socket = client_socket;
        req->request_json = strdup(buffer);
        queue_enqueue(request_queue, req);
    }
    return 0;
}