#include <iostream>
#include <thread>
#include <mutex>
#include <fstream>

#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/OFSTypes.h"
#include "../include/FileSystem.h"
#include "../include/UserMap.h"

using json = nlohmann::json;

OFSystem g_FileSystem;
std::mutex g_fs_mutex;

// Helper to check admin privileges
bool check_admin(const std::string& sid) {
    if (sid.empty()) return false;
    auto it = g_FileSystem.active_sessions.find(sid);
    return (it != g_FileSystem.active_sessions.end() && it->second->role == 1);
}

json handle_ofs_logic(json req) {
    json resp;
    std::string op = req["operation"];
    resp["operation"] = op;

    // --- 1. AUTHENTICATION ---
    if (op == "user_login") {
        std::string u = req["parameters"]["username"];
        std::string p = req["parameters"]["password"];
        std::string sid = login_user(g_FileSystem, u, p);
        if (!sid.empty()) {
            resp["status"] = "success";
            resp["data"]["session_id"] = sid;
            SessionInfo info = get_session_details(g_FileSystem, sid);
            resp["data"]["is_admin"] = (info.role == 1);
            resp["data"]["username"] = info.username;
        } else {
            resp["status"] = "error";
            resp["error_message"] = "Invalid credentials";
        }
        return resp;
    }

    // Check Session for all other commands
    std::string sid = req.value("session_id", "");
    if (g_FileSystem.active_sessions.find(sid) == g_FileSystem.active_sessions.end()) {
        resp["status"] = "error";
        resp["error_message"] = "Session expired or invalid";
        return resp;
    }

    bool is_admin = check_admin(sid);

    // --- 2. USER MANAGEMENT (Admin Only) ---
    if (op == "user_list") {
        if (!is_admin) return {{"status", "error"}, {"error_message", "Admin required"}};
        resp["status"] = "success";
        resp["data"]["users"] = list_all_users(g_FileSystem);
    }
    else if (op == "user_create") {
        if (!is_admin) return {{"status", "error"}, {"error_message", "Admin required"}};
        create_user(g_FileSystem, req["parameters"]["username"], req["parameters"]["password"], req["parameters"].value("role", 0));
        resp["status"] = "success";
    }
    else if (op == "user_delete") {
        if (!is_admin) return {{"status", "error"}, {"error_message", "Admin required"}};
        delete_user(g_FileSystem, req["parameters"]["username"]);
        resp["status"] = "success";
    }
    else if (op == "user_logout") {
        logout_user(g_FileSystem, sid);
        resp["status"] = "success";
    }

    // --- 3. SYSTEM ---
    else if (op == "fs_shutdown") {
        if (!is_admin) return {{"status", "error"}, {"error_message", "Admin required"}};
        resp["status"] = "success";
        // We will handle the actual exit in main() after sending response
    }
    else if (op == "get_fs_stats") {
        FSStats stats = get_fs_stats(g_FileSystem);
        resp["status"] = "success";
        resp["data"] = {
            {"total_size", stats.total_size},
            {"used_space", stats.used_space},
            {"free_space", stats.free_space},
            {"file_count", stats.file_count},
            {"dir_count", stats.directory_count}
        };
    }

    // --- 4. DIRECTORY OPERATIONS ---
    else if (op == "list_directory_contents") {
        std::string path = req["parameters"]["path"];
        auto entries = list_directory_contents(g_FileSystem, path);
        json list = json::array();
        for (const auto& e : entries) list.push_back({{"name", e.name}, {"is_directory", e.is_directory}});
        resp["status"] = "success";
        resp["data"] = list;
    }
    else if (op == "dir_create") {
        create_directory(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
    }
    else if (op == "remove_directory") {
        remove_directory(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
    }
    
    // --- 5. FILE OPERATIONS ---
    else if (op == "create_file_with_content") {
        create_file_with_content(g_FileSystem, req["parameters"]["path"], req["parameters"]["data"]);
        resp["status"] = "success";
    }
    else if (op == "file_read") {
        std::string content = read_file_content(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
        resp["data"]["content"] = content;
    }
    else if (op == "edit_file") {
        // Calls edit_file directly
        edit_file(g_FileSystem, req["parameters"]["path"], req["parameters"]["data"], req["parameters"]["index"]);
        resp["status"] = "success";
    }
    else if (op == "truncate_file_content") {
        truncate_file_content(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
    }
    else if (op == "remove_file") {
        remove_file(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
    }
    else if (op == "rename_path") {
        rename_path(g_FileSystem, req["parameters"]["old_path"], req["parameters"]["new_path"]);
        resp["status"] = "success";
    }

    // --- 6. METADATA & PERMISSIONS ---
    else if (op == "get_path_metadata") {
        FileMetadata meta = get_path_metadata(g_FileSystem, req["parameters"]["path"]);
        resp["status"] = "success";
        resp["data"] = {
            {"name", meta.name},
            {"size", meta.size},
            {"owner_id", meta.owner_id},
            {"permissions", meta.permissions},
            {"created", meta.created_time},
            {"modified", meta.modified_time},
            {"is_directory", meta.is_directory}
        };
    }
    else if (op == "set_path_permissions") {
        set_path_permissions(g_FileSystem, req["parameters"]["path"], req["parameters"]["permissions"]);
        resp["status"] = "success";
    }
    else {
        resp["status"] = "error";
        resp["error_message"] = "Unknown Operation";
    }

    return resp;
}

int main() {
    const std::string OMNI_FILE = "my_ofs.omni";
    std::ifstream f(OMNI_FILE);
    if (!f.good()) {
        format_filesystem(OMNI_FILE);
    }
    init_filesystem(g_FileSystem, OMNI_FILE);

    httplib::Server svr;
    svr.set_mount_point("/", "./www");

    svr.Post("/api", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto json_req = json::parse(req.body);
            std::lock_guard<std::mutex> lock(g_fs_mutex);
            
            json json_resp = handle_ofs_logic(json_req);
            res.set_content(json_resp.dump(), "application/json");

            // Handle shutdown request from within the main thread loop logic context
            if (json_resp["operation"] == "fs_shutdown" && json_resp["status"] == "success") {
                std::thread([](){ 
                    std::this_thread::sleep_for(std::chrono::seconds(1)); 
                    exit(0); 
                }).detach();
            }
        } 
        catch (std::exception& e) {
            json err = {{"status", "error"}, {"error_message", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    std::cout << "OFS Server running at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    
    user_map_destroy(g_FileSystem.user_map);
    return 0;
}