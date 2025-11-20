#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

#include "httplib.h"
#include "json.hpp"

extern "C" {
    #include "ofs_types.h"
    #include "hash_table.h"
    int fs_init(void** instance, const char* omni_path, const char* config_path);
    void fs_shutdown(void* instance);
    int user_login(void* instance, const char* username, const char* password, char** session_id);
    int user_logout(SessionInfo* session);
    int get_session_info(SessionInfo* session, SessionInfo* info_out);
    int user_create(OFSInstance* ofs, SessionInfo* admin_session, const char* username, const char* password, UserRole role);
    int user_delete(OFSInstance* ofs, SessionInfo* admin_session, const char* username);
    int user_list(OFSInstance* ofs, SessionInfo* admin_session, UserInfo** users_out, int* count_out);
    const char* get_error_message(int error_code);
    int dir_create(OFSInstance* ofs, SessionInfo* session, const char* path);
    int dir_list(OFSInstance* ofs, SessionInfo* session, const char* path, FileEntry** entries_out, int* count_out);
    int dir_delete(OFSInstance* ofs, SessionInfo* session, const char* path);
    int file_create(OFSInstance* ofs, SessionInfo* session, const char* path, const char* data, size_t size);
    int file_read(OFSInstance* ofs, SessionInfo* session, const char* path, char** buffer_out, size_t* size_out);
    int file_delete(OFSInstance* ofs, SessionInfo* session, const char* path);
    int file_rename(OFSInstance* ofs, SessionInfo* session, const char* old_path, const char* new_path);
}

using json = nlohmann::json;
static void* g_instance = NULL;

void add_cors_headers(httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Session-ID");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
}

SessionInfo* get_session_from_id(const std::string& session_id) {
    if (session_id.empty() || !g_instance) return NULL;
    OFSInstance* ofs = (OFSInstance*)g_instance;
    SessionInfo* session = (SessionInfo*)ht_get(ofs->sessions, session_id.c_str());
    if (session && session->is_valid) return session;
    return NULL;
}

int main() {
    if (fs_init(&g_instance, "compiled/sample.omni", "default.uconf") != OFS_SUCCESS) {
        printf("fs_init failed. Run 'make format' if the file system doesn't exist.\n");
        return 1;
    }
    printf("File system loaded successfully!\n");

    httplib::Server svr;
    svr.set_mount_point("/", "./source/ui");
    
    svr.Options("/.*", [](const httplib::Request&, httplib::Response &res){
        add_cors_headers(res);
        res.status = 204;
    });

    auto is_logged_in = [&](const httplib::Request& req, httplib::Response& res) -> bool {
        if (!get_session_from_id(req.get_header_value("X-Session-ID"))) {
            add_cors_headers(res);
            res.status = 401;
            res.set_content(json{{"status", "error"}, {"error_message", "Authentication required."}}.dump(), "application/json");
            return false;
        }
        return true;
    };

    auto is_admin = [&](const httplib::Request& req, httplib::Response& res) -> bool {
        SessionInfo* session = get_session_from_id(req.get_header_value("X-Session-ID"));
        if (!session) {
            add_cors_headers(res);
            res.status = 401;
            res.set_content(json{{"status", "error"}, {"error_message", "Authentication required."}}.dump(), "application/json");
            return false;
        }
        if (session->role != ROLE_ADMIN) {
            add_cors_headers(res);
            res.status = 403;
            res.set_content(json{{"status", "error"}, {"error_message", "Permission denied."}}.dump(), "application/json");
            return false;
        }
        return true;
    };

    svr.Post("/user_login", [](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /user_login ---\n");
        add_cors_headers(res);
        json params = json::parse(req.body);
        std::string username = params["username"];
        std::string password = params["password"];
        char* session_id_c_str = NULL;
        int result = user_login(g_instance, username.c_str(), password.c_str(), &session_id_c_str);
        if (result == OFS_SUCCESS && session_id_c_str) {
            res.set_content(json{{"status", "success"}, {"data", {{"session_id", session_id_c_str}}}}.dump(), "application/json");
            free(session_id_c_str);
        } else {
            res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });
    
    svr.Post("/user_logout", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /user_logout ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        SessionInfo* session = get_session_from_id(req.get_header_value("X-Session-ID"));
        user_logout(session);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Get("/get_session_info", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /get_session_info ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        SessionInfo session_info;
        get_session_info(get_session_from_id(req.get_header_value("X-Session-ID")), &session_info);
        res.set_content(json{{"status", "success"}, {"data", {{"username", session_info.username}, {"role", session_info.role}}}}.dump(), "application/json");
    });

    svr.Post("/dir_list", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /dir_list ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        
        // --- FIX: DECLARE VARIABLES HERE ---
        FileEntry* entries = NULL;
        int count = 0;

        int result = dir_list((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str(), &entries, &count);
        if (result == OFS_SUCCESS) {
            json data_array = json::array();
            for (int i=0; i < count; i++) {
                // --- FIX: Use a more compatible way to create the JSON object ---
                json item;
                item["name"] = entries[i].name;
                item["is_directory"] = (bool)entries[i].is_directory;
                data_array.push_back(item);
            }
            if(entries) free(entries);
            res.set_content(json{{"status", "success"}, {"data", data_array}}.dump(), "application/json");
        } else {
            res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/dir_create", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /dir_create ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        int result = dir_create((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "Directory created."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/file_create", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /file_create ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        std::string data = params.value("data", "");
        int result = file_create((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str(), data.c_str(), data.length());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "File created."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/file_read", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /file_read ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        char* buffer = NULL;
        size_t size = 0;
        int result = file_read((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str(), &buffer, &size);
        if (result == OFS_SUCCESS) {
            res.set_content(json{{"status", "success"}, {"data", {{"content", std::string(buffer, size)}}}}.dump(), "application/json");
            if(buffer) free(buffer);
        } else {
            res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/file_delete", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /file_delete ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        int result = file_delete((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "File deleted."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/dir_delete", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /dir_delete ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string path = params["path"];
        int result = dir_delete((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), path.c_str());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "Directory deleted."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });
    
    svr.Post("/file_rename", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /file_rename ---\n");
        add_cors_headers(res);
        if (!is_logged_in(req, res)) return;
        json params = json::parse(req.body);
        std::string old_path = params["old_path"];
        std::string new_path = params["new_path"];
        int result = file_rename((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), old_path.c_str(), new_path.c_str());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "Item renamed."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/user_create", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /user_create ---\n");
        add_cors_headers(res);
        if (!is_admin(req, res)) return;
        json params = json::parse(req.body);
        std::string username = params["username"];
        std::string password = params["password"];
        int role = params["role"];
        int result = user_create((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), username.c_str(), password.c_str(), (UserRole)role);
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "User created."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Post("/user_delete", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /user_delete ---\n");
        add_cors_headers(res);
        if (!is_admin(req, res)) return;
        json params = json::parse(req.body);
        std::string username = params["username"];
        int result = user_delete((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), username.c_str());
        if (result == OFS_SUCCESS) {
             res.set_content(json{{"status", "success"}, {"data", {{"message", "User deleted."}}}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    svr.Get("/user_list", [&](const httplib::Request& req, httplib::Response& res) {
        printf("\n--- Received request for /user_list ---\n");
        add_cors_headers(res);
        if (!is_admin(req, res)) return;
        UserInfo* users = NULL;
        int count = 0;
        int result = user_list((OFSInstance*)g_instance, get_session_from_id(req.get_header_value("X-Session-ID")), &users, &count);
        if (result == OFS_SUCCESS) {
            json user_array = json::array();
            for (int i=0; i < count; i++) {
                json user_item;
                user_item["user_id"] = users[i].user_id;
                user_item["username"] = users[i].username;
                user_item["role"] = users[i].role;
                user_array.push_back(user_item);
            }
            if(users) free(users);
            res.set_content(json{{"status", "success"}, {"data", user_array}}.dump(), "application/json");
        } else {
             res.set_content(json{{"status", "error"}, {"error_message", get_error_message(result)}}.dump(), "application/json");
        }
    });

    printf("HTTP server is starting on http://localhost:8080\n");
    svr.listen("0.0.0.0", 8080);
    
    printf("Shutting down gracefully...\n");
    if (g_instance) {
        fs_shutdown(g_instance);
    }
    return 0;
}